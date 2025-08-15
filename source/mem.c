#include "mem.h"
#include "util.h"

#include <stdio.h>

void writeMem(u64 offset, u64 size, u8* val)
{
    R_DEBUG(svcWriteDebugProcessMemory(debughandle, val, offset, size));
}

void readMem(u64 offset, u64 size, u8* out)
{
    R_DEBUG(svcReadDebugProcessMemory(out, debughandle, offset, size));
}