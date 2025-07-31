#include "mem.h"

#include <stdio.h>

void writeMem(u64 offset, u64 size, u8* val)
{
	Result rc = svcWriteDebugProcessMemory(debughandle, val, offset, size);
    if (R_FAILED(rc))
        printf("svcWriteDebugProcessMemory: %d\n", rc);
}

void readMem(u64 offset, u64 size, u8* out)
{
	Result rc = svcReadDebugProcessMemory(out, debughandle, offset, size);
    if (R_FAILED(rc))
        printf("svcReadDebugProcessMemory: %d\n", rc);
}