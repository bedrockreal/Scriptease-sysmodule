#include <switch.h>
#include <stdio.h>

// registered by lua
void attach();
void detach();
void detachController();

// metadata
u64 getPID();
u64 getMainNsoBase(u64 pid);
u64 getHeapBase(Handle handle);
u64 getTitleId(u64 pid);
u64 getTitleVersion(u64 pid);
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

void cancelTouch();

void screenOff();
void screenOn();

u32 getBatteryChargePercentage();

// not registered by lua
u64 followMainPointer(s64* jumps, size_t count);
u64 getoutsize(NsApplicationControlData* buf);

void setStickState(int side, int dxVal, int dyVal);
void reverseArray(u8* arr, int start, int end);
// void touch(int x, int y);
void touchMulti(HidTouchState* state, u64 sequentialCount, u64 holdTime, bool hold, u8* token);
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