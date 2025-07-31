#include <switch.h>
#include <stdio.h>

// registered by lua

bool getIsProgramOpen(u64 id);

void cancelTouch();

// not registered by lua
void touchMulti(HidTouchState* state, u64 sequentialCount, u64 holdTime, bool hold, u8* token);
void setKeyboardState(u64 modifiers, u64* keys);