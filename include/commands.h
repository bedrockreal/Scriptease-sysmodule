#include <switch.h>
#include <stdio.h>

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
extern ViDisplay disp;
extern Event vsyncEvent;

extern HiddbgHdlsSessionId sessionId;
extern u8 *workmem;
extern size_t workmem_size;

// typedef struct {
//     u64 main_nso_base;
//     u64 heap_base;
//     u64 titleID;
//     u64 titleVersion;
//     u8 buildID[0x20];
// } MetaData;

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

#define JOYSTICK_LEFT 0
#define JOYSTICK_RIGHT 1

// registered by lua
void attach();
void detach();
void detachController();

// metadata
u64 getPID();
u64 getMainNsoBase(u64 pid);
u64 getHeapBase(Handle handle);
u64 getTitleId(u64 pid);
u64 GetTitleVersion(u64 pid);
void getBuildID(u8* out, u64 pid); // -> lua_getBuildID
bool getIsProgramOpen(u64 id);

void readMem(u8* out, u64 offset, u64 size); // -> lua_peek()
void writeMem(u64 offset, u64 size, u8* val); // -> lua_poke()

void click(HidNpadButton btn);
void press(HidNpadButton btn);
void release(HidNpadButton btn);
bool getIsPaused();
void advanceFrames(int cnt);
void setControllerState(HidNpadButton btnState, int joy_l_x, int joy_l_y, int joy_r_x, int joy_r_y);
void resetControllerState();
void loadTAS(const char* arg);
void cancelTAS();

void screenOff();
void screenOn();

// not registered by lua
u64 followMainPointer(s64* jumps, size_t count);
u64 getoutsize(NsApplicationControlData* buf);

void setStickState(int side, int dxVal, int dyVal);
void reverseArray(u8* arr, int start, int end);
void touch(HidTouchState* state, u64 sequentialCount, u64 holdTime, bool hold, u8* token);
void key(HiddbgKeyboardAutoPilotState* states, u64 sequentialCount);
void clickSequence(char* seq, u8* token);

// comment this
void poke(u64 offset, u64 size, u8* val);
void peek(u64 offset, u64 size);
void peekInfinite(u64 offset, u64 size);
void peekMulti(u64* offset, u64* size, u64 count);

void freezeShort(u64 addr, s16 val);
void freezeInt(u64 addr, s32 val);
void freezeLongLong(u64 addr, s64 val);
void freezeFloat(u64 addr, float val);
void freezeDouble(u64 addr, double val);
void freezeLongDouble(u64 addr, long double val);