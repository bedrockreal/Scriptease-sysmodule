#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <switch.h>
#include "util.h"
#include "commands.h"
#include "common.h"

const char* const translate_keys[16] =
{
    "KEY_A\0",
    "KEY_B\0",
    "KEY_X\0", 
    "KEY_Y\0",
    "KEY_LSTICK\0", 
    "KEY_RSTICK\0",
    "KEY_L\0",
    "KEY_R\0",
    "KEY_ZL\0",
    "KEY_ZR\0",
    "KEY_PLUS\0",
    "KEY_MINUS\0",
    "KEY_DLEFT\0",
    "KEY_DUP\0",
    "KEY_DRIGHT\0",
    "KEY_DDOWN\0"
};

// taken from sys-httpd (thanks jolan!)
static const HidsysNotificationLedPattern breathingpattern = {
    .baseMiniCycleDuration = 0x8, // 100ms.
    .totalMiniCycles = 0x2,       // 3 mini cycles. Last one 12.5ms.
    .totalFullCycles = 0x2,       // 2 full cycles.
    .startIntensity = 0x2,        // 13%.
    .miniCycles = {
        // First cycle
        {
            .ledIntensity = 0xF,      // 100%.
            .transitionSteps = 0xF,   // 15 steps. Transition time 1.5s.
            .finalStepDuration = 0x0, // 12.5ms.
        },
        // Second cycle
        {
            .ledIntensity = 0x2,      // 13%.
            .transitionSteps = 0xF,   // 15 steps. Transition time 1.5s.
            .finalStepDuration = 0x0, // 12.5ms.
        },
    },
};

// beeg flash for wireless controller
static const HidsysNotificationLedPattern flashpattern = {
    .baseMiniCycleDuration = 0xF, // 200ms.
    .totalMiniCycles = 0x2,       // 3 mini cycles. Last one 12.5ms.
    .totalFullCycles = 0x2,       // 2 full cycles.
    .startIntensity = 0xF,        // 100%.
    .miniCycles = {
        // First and cloned cycle
        {
            .ledIntensity = 0xF,      // 100%.
            .transitionSteps = 0xF,   // 15 steps. Transition time 1.5s.
            .finalStepDuration = 0x0, // 12.5ms.
        },
        // clone
        {
            .ledIntensity = 0xF,      // 100%.
            .transitionSteps = 0xF,   // 15 steps. Transition time 1.5s.
            .finalStepDuration = 0x0, // 12.5ms.
        },
    },
};

int setupServerSocket(int port)
{
    int lissock;
    int yes = 1;
    struct sockaddr_in server;
    lissock = socket(AF_INET, SOCK_STREAM, 0);

    setsockopt(lissock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);

    while (bind(lissock, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        svcSleepThread(1e+9L);
    }
    listen(lissock, 3);
    return lissock;
}

u64 parseStringToInt(char* arg){
    if(strlen(arg) > 2){
        if(arg[1] == 'x'){
            u64 ret = strtoul(arg, NULL, 16);
            return ret;
        }
    }
    u64 ret = strtoul(arg, NULL, 10);
    return ret;
}

s64 parseStringToSignedLong(char* arg){
    if(strlen(arg) > 2){
        if(arg[1] == 'x' || arg[2] == 'x'){
            s64 ret = strtol(arg, NULL, 16);
            return ret;
        }
    }
    s64 ret = strtol(arg, NULL, 10);
    return ret;
}

u8* parseStringToByteBuffer(char* arg, u64* size)
{
    char toTranslate[3] = {0};
    int length = strlen(arg);
    bool isHex = false;

    if(length > 2){
        if(arg[1] == 'x'){
            isHex = true;
            length -= 2;
            arg = &arg[2]; //cut off 0x
        }
    }

    bool isFirst = true;
    bool isOdd = (length % 2 == 1);
    u64 bufferSize = length / 2;
    if(isOdd) bufferSize++;
    u8 *buffer = malloc(bufferSize);



    u64 i;
    for (i = 0; i < bufferSize; i++){
        if(isOdd){
            if(isFirst){
                toTranslate[0] = '0';
                toTranslate[1] = arg[i];
            }else{
                toTranslate[0] = arg[(2 * i) - 1];
                toTranslate[1] = arg[(2 * i)];
            }
        }else{
            toTranslate[0] = arg[i*2];
            toTranslate[1] = arg[(i*2) + 1];      
        }
        isFirst = false;
        if(isHex){
            buffer[i] = strtoul(toTranslate, NULL, 16);
        }else{
            buffer[i] = strtoul(toTranslate, NULL, 10);
        }
    }
    *size = bufferSize;
    return buffer;
}

HidNpadButton parseStringToButton(char* arg)
{
    for (int i = 0; i < 16; ++i)
    {
        if (strcmp(translate_keys[i], arg) == 0)
            return BIT(i);
    }

    if (strcmp(arg, "HOME") == 0)
    {
        return HiddbgNpadButton_Home;
    }
    else if (strcmp(arg, "CAPTURE") == 0)
    {
        return HiddbgNpadButton_Capture;
    }
    else if (strcmp(arg, "PALMA") == 0)
    {
        return HidNpadButton_Palma;
    }
    else if (strcmp(arg, "UNUSED") == 0) //Possibly useful for HOME button eaten issues
    {
        return BIT(20);
    }

    return HidNpadButton_A; //I guess lol
}

Result capsscCaptureForDebug(void *buffer, size_t buffer_size, u64 *size) {
    struct {
        u32 a;
        u64 b;
    } in = {0, 10000000000};
    return serviceDispatchInOut(capsscGetServiceSession(), 1204, in, *size,
        .buffer_attrs = {SfBufferAttr_HipcMapTransferAllowsNonSecure | SfBufferAttr_HipcMapAlias | SfBufferAttr_Out},
        .buffers = { { buffer, buffer_size } },
    );
}

static void sendPatternStatic(const HidsysNotificationLedPattern* pattern, const HidNpadIdType idType)
{
    s32 total_entries;
    HidsysUniquePadId unique_pad_ids[2]={0};

    Result rc = hidsysGetUniquePadsFromNpad(idType, unique_pad_ids, 2, &total_entries);
    if (R_FAILED(rc)) 
        return; // probably incompatible or no pads connected

    for (int i = 0; i < total_entries; i++)
        rc = hidsysSetNotificationLedPattern(pattern, unique_pad_ids[i]);
}

void flashLed()
{
    Result rc = hidsysInitialize();
    if (R_FAILED(rc))
        fatalThrow(rc);
    sendPatternStatic(&breathingpattern, HidNpadIdType_Handheld); // glow in and out x2 for docked joycons
    sendPatternStatic(&flashpattern, HidNpadIdType_No1); // big hard single glow for wireless/wired joycons or controllers
    hidsysExit();
}

int parseNxTasStr(int argc, char **argv)
{
    // size of ret should be 4
    // return 0 if success
    // at first, prev_frame = 0

    if (argc != 4) return 1;
    int cur_frame = strtol(argv[0], NULL, 0);
    if (cur_frame > prev_frame) advanceFrames(cur_frame - prev_frame);

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

    int stickPos[2][2];
    for (int i = 0; i < 2; ++i)
    {
        ptr = strtok(argv[2 + i], ";");
        for (int j = 0; j < 2; ++j)
        {
            if (ptr == NULL) return 1;
            stickPos[i][j] = strtol(ptr, NULL, 0);
            ptr = strtok(NULL, ";");
        }
    }

    setControllerState(btnState, stickPos[0][0], stickPos[0][1], stickPos[1][0], stickPos[1][1]);
    advanceFrames(1);
    resetControllerState();
    prev_frame = cur_frame + 1;

    return 0;
}