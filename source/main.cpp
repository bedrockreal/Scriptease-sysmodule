#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <poll.h>
#include <errno.h>

#include "commands.h"
#include "args.h"
#include "util.h"
#include "freeze.h"
#include "common.h"

Thread freezeThread, touchThread, keyboardThread, clickThread, tasThread;
FILE* tas_script = NULL;

// locks for thread
Mutex freezeMutex, touchMutex, keyMutex, clickMutex, tasMutex;

// events for releasing or idling threads
FreezeThreadState freeze_thr_state = Active; 
u8 clickThreadState = 0, tasThreadState = 0; // 1 = break thread
// key and touch events currently being processed
KeyData currentKeyEvent = {0};
TouchData currentTouchEvent = {0};
char* currentClick = NULL;

// for cancelling the touch/click thread
u8 touchToken = 0;
u8 clickToken = 0;

// fd counters and max size
int fd_count = 0;
int fd_size = 5;

// we aren't an applet
u32 __nx_applet_type = AppletType_None;

// Needed for TAS
int prev_frame = 0;
int line_cnt = 0;

u64 freezeRate = 3;

struct pollfd *pfds;

ViDisplay disp;
Event vsyncEvent;

// we override libnx internals to do a minimal init
void __libnx_initheap(void)
{
	static u8 inner_heap[HEAP_SIZE];
    extern void* fake_heap_start;
    extern void* fake_heap_end;

    // Configure the newlib heap.
    fake_heap_start = inner_heap;
    fake_heap_end   = inner_heap + sizeof(inner_heap);
}

void __appInit(void)
{
    Result rc;
    svcSleepThread(20000000000L);
    rc = smInitialize();
    if (R_FAILED(rc))
        fatalThrow(rc);
    if (hosversionGet() == 0) {
        rc = setsysInitialize();
        if (R_SUCCEEDED(rc)) {
            SetSysFirmwareVersion fw;
            rc = setsysGetFirmwareVersion(&fw);
            if (R_SUCCEEDED(rc))
                hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
            setsysExit();
        }
    }
    rc = pmdmntInitialize();
	if (R_FAILED(rc)) 
        fatalThrow(rc);
    rc = ldrDmntInitialize();
	if (R_FAILED(rc)) 
		fatalThrow(rc);
    rc = pminfoInitialize();
	if (R_FAILED(rc)) 
		fatalThrow(rc);
    rc = socketInitializeDefault();
    if (R_FAILED(rc))
        fatalThrow(rc);
    rc = capsscInitialize();
    if (R_FAILED(rc))
        fatalThrow(rc);
    rc = viInitialize(ViServiceType_System);
    if (R_FAILED(rc))
        fatalThrow(rc);

    // adopted from usb-botbase
    rc = fsInitialize();
    if (R_FAILED(rc))
        fatalThrow(rc);

    rc = fsdevMountSdmc();
    if (R_FAILED(rc))
        fatalThrow(rc);
}

void __attribute__((weak)) userAppExit(void);

void __appExit(void)
{
    // if (R_SUCCEEDED(rc))
    {
	    freeze_thr_state = Exit;
		threadWaitForExit(&freezeThread);
        threadClose(&freezeThread);

        currentTouchEvent.state = 3;
        threadWaitForExit(&touchThread);
        threadClose(&touchThread);

        currentKeyEvent.state = 3;
        threadWaitForExit(&keyboardThread);
        threadClose(&keyboardThread);
        
        clickThreadState = 1;
        threadWaitForExit(&clickThread);
        threadClose(&clickThread);
	}
	
	clearFreezes();
    freeFreezes();

    smExit();
    nsExit();
    audoutExit();
    socketExit();

    eventClose(&vsyncEvent);
    viCloseDisplay(&disp);
    viExit();

    fsExit();
    fsdevUnmountAll();
}

void add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size)
{
    if (*fd_count == *fd_size) {
        *fd_size *= 2;

        *pfds = static_cast<pollfd*>(realloc(*pfds, sizeof(**pfds) * (*fd_size)));
    }

    (*pfds)[*fd_count].fd = newfd;
    (*pfds)[*fd_count].events = POLLIN;

    (*fd_count)++;
}

void del_from_pfds(struct pollfd pfds[], int i, int *fd_count)
{
    pfds[i] = pfds[*fd_count-1];

    (*fd_count)--;
}

void makeTouch(HidTouchState* state, u64 sequentialCount, u64 holdTime, bool hold)
{
    mutexLock(&touchMutex);
    memset(&currentTouchEvent, 0, sizeof currentTouchEvent);
    currentTouchEvent.states = state;
    currentTouchEvent.sequentialCount = sequentialCount;
    currentTouchEvent.holdTime = holdTime;
    currentTouchEvent.hold = hold;
    currentTouchEvent.state = 1;
    mutexUnlock(&touchMutex);
}

void makeKeys(HiddbgKeyboardAutoPilotState* states, u64 sequentialCount)
{
    mutexLock(&keyMutex);
    memset(&currentKeyEvent, 0, sizeof currentKeyEvent);
    currentKeyEvent.states = states;
    currentKeyEvent.sequentialCount = sequentialCount;
    currentKeyEvent.state = 1;
    mutexUnlock(&keyMutex);
}

#ifdef __cplusplus
}

#include "lua_wrap.hpp"
#endif

/*
int argmain(int argc, char **argv)
{
    if (argc == 0)
        return 0;
	
    //touch followed by arrayof: <x in the range 0-1280> <y in the range 0-720>. Array is sequential taps, not different fingers. Functions in its own thread, but will not allow the call again while running. tapcount * pollRate * 2
    if (!strcmp(argv[0], "touch"))
	{
        if(argc < 3 || argc % 2 == 0)
            return 0;

        u32 count = (argc-1)/2;
		
        #ifdef __cplusplus
        HidTouchState* state = static_cast<HidTouchState*>(calloc(count, sizeof(HidTouchState)));
        #else
        HidTouchState* state = calloc(count, sizeof(HidTouchState));
        #endif

        u32 i, j = 0;
        for (i = 0; i < count; ++i)
        {
            state[i].diameter_x = state[i].diameter_y = fingerDiameter;
            state[i].x = (u32) parseStringToInt(argv[++j]);
            state[i].y = (u32) parseStringToInt(argv[++j]);
        }

        makeTouch(state, count, pollRate * 1e+6L, false);
	}

    //touchHold <x in the range 0-1280> <y in the range 0-720> <time in milliseconds (must be at least 15ms)>. Functions in its own thread, but will not allow the call again while running. pollRate + holdtime
    if(!strcmp(argv[0], "touchHold")){
        if(argc != 4)
            return 0;

        #ifdef __cplusplus
        HidTouchState* state = static_cast<HidTouchState*>(calloc(1, sizeof(HidTouchState)));
        #else
        HidTouchState* state = calloc(1, sizeof(HidTouchState));
        #endif

        state->diameter_x = state->diameter_y = fingerDiameter;
        state->x = (u32) parseStringToInt(argv[1]);
        state->y = (u32) parseStringToInt(argv[2]);
        u64 time = parseStringToInt(argv[3]);
        makeTouch(state, 1, time * 1e+6L, false);
    }

    //touchDraw followed by arrayof: <x in the range 0-1280> <y in the range 0-720>. Array is vectors of where finger moves to, then removes the finger. Functions in its own thread, but will not allow the call again while running. (vectorcount * pollRate * 2) + pollRate
    if (!strcmp(argv[0], "touchDraw"))
	{
        if(argc < 3 || argc % 2 == 0)
            return 0;

        u32 count = (argc-1)/2;
		
        #ifdef __cplusplus
        HidTouchState* state = static_cast<HidTouchState*>(calloc(count, sizeof(HidTouchState)));
        #else
        HidTouchState* state = calloc(count, sizeof(HidTouchState));
        #endif

        u32 i, j = 0;
        for (i = 0; i < count; ++i)
        {
            state[i].diameter_x = state[i].diameter_y = fingerDiameter;
            state[i].x = (u32) parseStringToInt(argv[++j]);
            state[i].y = (u32) parseStringToInt(argv[++j]);
        }

        makeTouch(state, count, pollRate * 1e+6L * 2, true);
	}

    if (!strcmp(argv[0], "touchCancel"))
        touchToken = 1;
    return 0;
}

*/

int main()
{
    Result rc = viOpenDefaultDisplay(&disp);
    if(R_FAILED(rc))
        fatalThrow(rc);
    rc = viGetDisplayVsyncEvent(&disp, &vsyncEvent);
    if(R_FAILED(rc))
        fatalThrow(rc);
    
    char *linebuf = static_cast<char*>(malloc(sizeof(char) * MAX_LINE_LENGTH));
    pfds = static_cast<pollfd*>(malloc(sizeof *pfds * fd_size));

    int c = sizeof(struct sockaddr_in);
    struct sockaddr_in client;

    int listenfd = setupServerSocket(6000);
    pfds[0].fd = listenfd;
    pfds[0].events = POLLIN;
    fd_count = 1;

    int clientfd;
	int fr_count = 0;
	
    initFreezes();

	// freeze thread
	mutexInit(&freezeMutex);
	rc = threadCreate(&freezeThread, sub_freeze, (void*)&freeze_thr_state, NULL, THREAD_SIZE, 0x2C, -2); 
	if (R_SUCCEEDED(rc))
        rc = threadStart(&freezeThread);

    // touch thread
    mutexInit(&touchMutex);
    rc = threadCreate(&touchThread, sub_touch, (void*)&currentTouchEvent, NULL, THREAD_SIZE, 0x2C, -2); 
    if (R_SUCCEEDED(rc))
        rc = threadStart(&touchThread);

    // key thread
    mutexInit(&keyMutex);
    rc = threadCreate(&keyboardThread, sub_key, (void*)&currentKeyEvent, NULL, THREAD_SIZE, 0x2C, -2); 
    if (R_SUCCEEDED(rc))
        rc = threadStart(&keyboardThread);

    // TAS thread (culprit)
    mutexInit(&tasMutex);

    sol::state lua;
    luaInit(lua);
    flashLed();

    while (true)
    {
        poll(pfds, fd_count, -1);
		mutexLock(&freezeMutex);
        for(int i = 0; i < fd_count; i++) 
        {
            if (pfds[i].revents & POLLIN) 
            {
                if (pfds[i].fd == listenfd) 
                {
                    clientfd = accept(listenfd, (struct sockaddr *)&client, (socklen_t *)&c);
                    if(clientfd != -1)
                    {
                        add_to_pfds(&pfds, clientfd, &fd_count, &fd_size);
                    }
                    else{
                        svcSleepThread(1e+9L);
                        close(listenfd);
                        listenfd = setupServerSocket(6000);
                        pfds[0].fd = listenfd;
                        pfds[0].events = POLLIN;
                        break;
                    }
                }
                else
                {
                    bool readEnd = false;
                    int readBytesSoFar = 0;
                    while(!readEnd){
                        int len = read(pfds[i].fd, &linebuf[readBytesSoFar], 1);
                        if(len <= 0)
                        {
                            // end of input
                            close(pfds[i].fd);
                            del_from_pfds(pfds, i, &fd_count);
                            readEnd = true;
                        }
                        else
                        {
                            readBytesSoFar += len;
                            if(linebuf[readBytesSoFar-1] == '\n'){
                                // end of line
                                if (pfds[i].fd == clientfd)
                                {
                                    readEnd = true;
                                    dup2(pfds[i].fd, STDOUT_FILENO);

                                    try
                                    {
                                        if (linebuf[0] == '-' && linebuf[1] == '-')
                                        {
                                            linebuf[readBytesSoFar - 1] = 0;
                                            int offset = 2;
                                            for (; offset < readBytesSoFar && linebuf[offset] == ' '; ++offset);
                                            lua.safe_script_file(std::string(linebuf + offset));
                                        }
                                        else lua.safe_script(linebuf);
                                    }
                                    catch(const sol::error& e)
                                    {
                                        printf("%s\n", e.what());
                                    }
                                    fflush(stdout);

                                    // update configure
                                    buttonClickSleepTime = lua.get_or("clickWait", 50);
                                    pollRate = lua.get_or("pollUpd", 17);
                                    freezeRate = lua.get_or("freezeUpd", 3);
                                    frameAdvanceWaitTimeNs = lua.get_or("advWait", 1e7);
                                    fingerDiameter = lua.get_or("fingerDiameter", 50);
                                    keyPressSleepTime = lua.get_or("keyWait", 50);

                                    memset(linebuf, 0, readBytesSoFar);
                                }
                                else { }
                            }
                        }
                    }
                }
            }
        }
		fr_count = getFreezeCount();
		if (fr_count == 0)
			freeze_thr_state = Idle;
		mutexUnlock(&freezeMutex);

        lua.set("fdCount", fd_count);

        rc = eventWait(&vsyncEvent, 0xFFFFFFFFFFF);
        if(R_FAILED(rc))
            fatalThrow(rc);
    }
	
    return 0;
}

void sub_freeze(void *arg)
{
	u64 heap_base;
	u64 tid_now = 0;
	u64 pid = 0;
	bool wait_su = false;
	int freezecount = 0;
	
	IDLE:
    while (freezecount == 0)
	{
		if (*(FreezeThreadState*)arg == Exit)
			break;
		
		// do nothing
		svcSleepThread(1e+9L);
		freezecount = getFreezeCount();
	}
	
	while (1)
	{
		if (*(FreezeThreadState*)arg == Exit)
			break;
		
		if (*(FreezeThreadState*)arg == Idle) // no freeze
		{
			mutexLock(&freezeMutex);
			freeze_thr_state = Active;
			mutexUnlock(&freezeMutex); // stupid but it works so is it really stupid? (yes)
			freezecount = 0;
			wait_su = false;
			goto IDLE;
		}
        else if (*(FreezeThreadState*)arg == Pause)
        {
            svcSleepThread(1e+8L); //1s
            continue;
        }
		
		mutexLock(&freezeMutex);
		attach();
		heap_base = getHeapBase(debughandle);
		pmdmntGetApplicationProcessId(&pid);
		tid_now = getTitleId(pid);
		detach();
		
		// don't freeze on startup of new tid to remove any chance of save corruption
		if (tid_now == 0)
		{
			mutexUnlock(&freezeMutex);
			svcSleepThread(1e+10L);
			wait_su = true;
			continue;
		}
		
		if (wait_su)
		{
			mutexUnlock(&freezeMutex);
			svcSleepThread(3e+10L);
			wait_su = false;
			mutexLock(&freezeMutex);
		}
		
		if (heap_base > 0)
		{
			attach();
			for (int j = 0; j < FREEZE_DIC_LENGTH; j++)
			{
				if (freezes[j].state == 1 && freezes[j].titleId == tid_now)
				{
					writeMem(heap_base + freezes[j].address, freezes[j].size, freezes[j].vData);
				}
			}
			detach();
		}
		
		mutexUnlock(&freezeMutex);
		svcSleepThread(freezeRate * 1e+6L);
		tid_now = 0;
		pid = 0;
	}
}

void sub_touch(void *arg)
{
    while (1)
    {
        TouchData* touchPtr = (TouchData*)arg;
        if (touchPtr->state == 1)
        {
            mutexLock(&touchMutex); // don't allow any more assignments to the touch var (will lock the main thread)
            touchMulti(touchPtr->states, touchPtr->sequentialCount, touchPtr->holdTime, touchPtr->hold, &touchToken);
            free(touchPtr->states);
            touchPtr->state = 0;
            mutexUnlock(&touchMutex);
        }

        svcSleepThread(1e+6L);
        
        touchToken = 0;

        if (touchPtr->state == 3)
            break;
    }
}

void sub_key(void *arg)
{
    while (1)
    {
        KeyData* keyPtr = (KeyData*)arg;
        if (keyPtr->state == 1)
        {
            mutexLock(&keyMutex); 
            key(keyPtr->states, keyPtr->sequentialCount);
            free(keyPtr->states);
            keyPtr->state = 0;
            mutexUnlock(&keyMutex);
        }

        svcSleepThread(1e+6L);

        if (keyPtr->state == 3)
            break;
    }
}

void sub_tas(void *arg)
{
    if (tas_script == NULL) return;
    if (!getIsPaused()) attach();

    prev_frame = 0;
    constexpr int NXTAS_MAX_LINE_LEN = 256;
    char line[NXTAS_MAX_LINE_LEN];
    int line_cnt = 0;

    while (fgets(line, NXTAS_MAX_LINE_LEN, tas_script) != NULL)
    {
        int ret = parseArgs(line, &parseNxTasStr);
        if (ret != 0)
        {
            printf("Error parsing line %d\n", line_cnt);
            printf("%s\n", line);
            detach();
            break;
        }
        line_cnt++;

        if (tasThreadState == 1) break;
    }
    fclose(tas_script);
    tas_script = NULL;
    resetControllerState();
}
