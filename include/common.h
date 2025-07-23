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

typedef struct {
    HidTouchState* states;
    u64 sequentialCount;
    u64 holdTime;
    bool hold;
    u8 state;
} TouchData;

typedef struct {
    HiddbgKeyboardAutoPilotState* states;
    u64 sequentialCount;
    u8 state;
} KeyData;

extern Thread freezeThread, touchThread, keyboardThread, clickThread, tasThread;

// prototype thread functions to give the illusion of cleanliness
void sub_freeze(void *arg);
void sub_touch(void *arg);
void sub_key(void *arg);
void sub_tas(void *arg);

// make utils
void makeTouch(HidTouchState* state, u64 sequentialCount, u64 holdTime, bool hold);
void makeKeys(HiddbgKeyboardAutoPilotState* states, u64 sequentialCount);

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

// we aren't an applet
extern u32 __nx_applet_type;

// Needed for TAS
extern int prev_frame;
extern int line_cnt;

extern u64 freezeRate;

extern struct pollfd *pfds;

// Screen
extern ViDisplay disp;
extern Event vsyncEvent;

// Keys
extern const char* const translate_keys[16];

// commands.h
extern Handle debughandle;
extern bool bControllerIsInitialised;
extern HidDeviceType controllerInitializedType;
extern HiddbgHdlsHandle controllerHandle;
extern HiddbgHdlsDeviceInfo controllerDevice;
extern HiddbgHdlsState controllerState;
extern HiddbgKeyboardAutoPilotState dummyKeyboardState;
extern u64 buttonClickSleepTime;
extern u64 keyPressSleepTime;
extern u64 pollRate;
extern u32 fingerDiameter;

extern u32 frameAdvanceWaitTimeNs;

extern HiddbgHdlsSessionId sessionId;
extern u8 *workmem;
extern size_t workmem_size;

#define JOYSTICK_LEFT 0
#define JOYSTICK_RIGHT 1