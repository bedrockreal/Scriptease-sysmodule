#pragma once

#include <switch.h>

typedef enum {
    Active = 0,
    Exit = 1,
    Idle = 2,
    Pause = 3
} FreezeThreadState;

typedef struct {
    HidTouchState* states;
    u64 sequentialCount;
    u64 holdTime;
    bool hold;
    u8 state;
} TouchData;

// make utils
void makeTouch(HidTouchState* state, u64 sequentialCount, u64 holdTime, bool hold);


// events for releasing or idling threads
extern FreezeThreadState freeze_thr_state; 
// key and touch events currently being processed
extern TouchData currentTouchEvent;

// for cancelling the touch thread
extern u8 touchToken;

// fd counters and max size
extern int fd_count;
extern int fd_size;

extern u64 freezeRate;
extern u32 fingerDiameter;