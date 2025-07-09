#pragma once

#include <switch.h>

#define TITLE_ID 0x430000000000000B
#define HEAP_SIZE 0x00400000
#define THREAD_SIZE 0x1A000
#define VERSION_S "2.4"

typedef enum {
    Active = 0,
    Exit = 1,
    Idle = 2,
    Pause = 3
} FreezeThreadState;

extern Thread freezeThread, touchThread, keyboardThread, clickThread, tasThread;

// prototype thread functions to give the illusion of cleanliness
void sub_freeze(void *arg);
void sub_touch(void *arg);
void sub_key(void *arg);
void sub_click(void *arg);
void sub_tas(void *arg);

extern FILE* tas_script;

// locks for thread
extern Mutex freezeMutex, touchMutex, keyMutex, clickMutex, tasMutex;

// events for releasing or idling threads
extern FreezeThreadState freeze_thr_state; 
extern u8 clickThreadState;
extern u8 tasThreadState; // 1 = break thread
// key and touch events currently being processed
extern KeyData currentKeyEvent;
extern TouchData currentTouchEvent;
extern char* currentClick;

// for cancelling the touch/click thread
extern u8 touchToken;
extern u8 clickToken;

// fd counters and max size
extern int fd_count;
extern int fd_size;
extern int tasfilefd;

// we aren't an applet
extern u32 __nx_applet_type;

// Needed for TAS
extern int prev_frame;
extern int line_cnt;

extern u64 mainLoopSleepTime;
extern u64 freezeRate;
extern bool debugResultCodes;
extern bool echoCommands;
extern struct pollfd *pfds;

extern const char* const translate_keys[16];