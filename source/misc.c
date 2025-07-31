#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#include "misc.h"
#include "util.h"
#include "common.h"
#include "freeze.h"

Handle debughandle = 0;
u32 fingerDiameter = 50;

bool getIsProgramOpen(u64 id)
{
    u64 pid = 0;    
    Result rc = pmdmntGetProcessId(&pid, id);
    if (pid == 0 || R_FAILED(rc))
        return false;

    return true;
}

void touchMulti(HidTouchState* state, u64 sequentialCount, u64 holdTime, bool hold, u8* token)
{
    // initController();
    // state->delta_time = holdTime; // only the first touch needs this for whatever reason
    // for (u32 i = 0; i < sequentialCount; i++)
    // {
    //     hiddbgSetTouchScreenAutoPilotState(&state[i], 1);
    //     svcSleepThread(holdTime);
    //     if (!hold)
    //     {
    //         hiddbgSetTouchScreenAutoPilotState(NULL, 0);
    //         svcSleepThread(pollRate * 1e+6L);
    //     }

    //     if ((*token) == 1)
    //         break;
    // }

    // if(hold) // send finger release event
    // {
    //     hiddbgSetTouchScreenAutoPilotState(NULL, 0);
    //     svcSleepThread(pollRate * 1e+6L);
    // }
    
    // hiddbgUnsetTouchScreenAutoPilotState();
}

void cancelTouch() { touchToken = 1; }