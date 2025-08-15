#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdbool.h>
#include "util.h"
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
    R_ASSERT(hidsysInitialize());
    sendPatternStatic(&breathingpattern, HidNpadIdType_Handheld); // glow in and out x2 for docked joycons
    sendPatternStatic(&flashpattern, HidNpadIdType_No1); // big hard single glow for wireless/wired joycons or controllers
    hidsysExit();
}

void add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size)
{
    if (*fd_count == *fd_size) {
        *fd_size *= 2;

        *pfds = realloc(*pfds, sizeof(**pfds) * (*fd_size));
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

void reverseArray(u8* arr, int start, int end)
{
    int temp;
    while (start < end)
    {
        temp = arr[start];   
        arr[start] = arr[end];
        arr[end] = temp;
        start++;
        end--;
    }   
}

bool getIsProgramOpen(u64 tid)
{
    u64 pid = 0;    
    Result rc = pmdmntGetProcessId(&pid, tid);
    if (pid == 0 || R_FAILED(rc))
        return false;

    return true;
}