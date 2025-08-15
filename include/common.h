#pragma once

#include <switch.h>

typedef enum {
    Active = 0,
    Exit = 1,
    Idle = 2,
    Pause = 3
} FreezeThreadState;

// fd counters and max size
extern int fd_count;
extern int fd_size;

extern u64 freeze_rate;