#include "tas.hpp"

extern "C"
{
    #include "util.h"
    #include "meta.h"

    #include <stdlib.h>
    #include <string.h>
    #include <errno.h>
}

#include "control.hpp"

extern std::vector<bot> bots;


TAS::TAS()
{
    file = NULL;
    line_cnt = 1;
    frame_counter = 0;
}

TAS::~TAS() {kill(); }

void TAS::load(std::string file_name, bool auto_run)
{
    file = fopen(file_name.c_str(), "r");
    if (!file)
    {
        printf("Error opening file : %d (%s)\n", errno, strerror(errno));
    }
    is_paused = !auto_run;
    if (!readLine()) kill();
}

bool TAS::readLine()
{
    // kill if return 1
    char line[MAX_LINE_LEN];
    if (fgets(line, MAX_LINE_LEN, file) == NULL) return 1;
    nxt_line = parseNxTasStr(line);
    if (nxt_line[0] == -1)
    {
        printf("Error parsing line %d\n", line_cnt);
        printf("%s\n", line);
        return 1;
    }
    ++line_cnt;
    return 0;
}

void TAS::kill()
{
    if (file)
    {
        fclose(file);
        file = NULL;
    }
}

bool TAS::isActive() { return (file != NULL); }

u32 frame_advance_wait_time_ns = 1e7;

std::array<int, 6> parseNxTasStr(char *str)
{
    std::array<int, 6> out{-1, -1, -1, -1, -1, -1};
    std::array<int, 6> fail = out;

    char *argstr = (char*)malloc(strlen(str) + 1);
    memcpy(argstr, str, strlen(str) + 1);
    int argc = 0;

    char *tmpstr = (char*)malloc(strlen(argstr) + 1);
    memcpy(tmpstr, argstr, strlen(argstr) + 1);
    for (char *p = strtok(tmpstr, " \r\n"); p != NULL; p = strtok(NULL, " \r\n")) {
        argc++;
    }
    free(tmpstr);

    // expect argc >= 4, out_size >= 6
    // return 0 if success

    if (argc < 4) return fail;
    char **argv = (char**)malloc(sizeof(char *) * argc);
    int i = 0;
    for (char *p = strtok(argstr, " \r\n"); p != NULL; p = strtok(NULL, " \r\n")) 
        argv[i++] = p;

    out[0] = strtol(argv[0], NULL, 0);

    char *ptr = NULL;
    u64 btnState = 0;
    if (strcmp(argv[1], "NONE"))
    {
        ptr = strtok(argv[1], ";");
        while (ptr != NULL)
        {
            btnState |= (s32) parseStringToButton(ptr);
            ptr = strtok(NULL, ";");
        }
    }

    int stick_pos[2][2];
    for (int i = 0; i < 2; ++i)
    {
        ptr = strtok(argv[2 + i], ";");
        for (int j = 0; j < 2; ++j)
        {
            if (ptr == NULL) return fail;
            stick_pos[i][j] = strtol(ptr, NULL, 0);
            ptr = strtok(NULL, ";");
        }
    }

    out[1] = btnState;
    for (int i = 0; i < 2; ++i) for (int j = 0; j < 2; ++j)
        out[2 + 2 * i + j] = stick_pos[i][j];

    return out;
}

void attach()
{
    u64 pid = 0;
    Result rc = pmdmntGetApplicationProcessId(&pid);
    if (R_FAILED(rc))
        printf("pmdmntGetApplicationProcessId: %d\n", rc);

    detach();

    rc = svcDebugActiveProcess(&debughandle, pid);
    if (R_FAILED(rc))
        printf("svcDebugActiveProcess: %d\n", rc);
}

void detach(){
    if (getIsPaused())
    {
        svcCloseHandle(debughandle);
    }
}

bool getIsPaused() { return debughandle != 0; }

void advanceFrames(const int cnt)
{
    s32 cur_priority;
    svcGetThreadPriority(&cur_priority, CUR_THREAD_HANDLE);
    
    u64 pid = getPID();
    for (int i = 1; i <= cnt; ++i)
    {
        svcSetThreadPriority(CUR_THREAD_HANDLE, 10);
        R_ASSERT(eventWait(&vsyncEvent, 0xFFFFFFFFFFF));

        for (int i = 0; i < bots.size(); ++i) bots[i].updateScript();

        svcCloseHandle(debughandle);
        svcSleepThread(frame_advance_wait_time_ns);

        Result rc = svcDebugActiveProcess(&debughandle, pid);
        if (R_FAILED(rc))
            printf("svcDebugActiveProcess: %d\n", rc);

        svcSetThreadPriority(CUR_THREAD_HANDLE, cur_priority);
    }
}
