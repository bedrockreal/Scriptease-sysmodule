#include <switch.h>

extern Handle debughandle;

void readMem(u64 offset, u64 size, u8* out); // -> lua_peek()
void writeMem(u64 offset, u64 size, u8* val); // -> lua_poke()