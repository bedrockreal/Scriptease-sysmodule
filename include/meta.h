#include <switch.h>

extern Handle debughandle;

u64 getPID();
u64 getMainNsoBase(u64 pid);
u64 getHeapBase(Handle handle);
u64 getTitleId(u64 pid);
u64 getTitleVersion(u64 pid);
void getBuildID(u8* out, u64 pid); // -> lua_getBuildID
u64 getoutsize(NsApplicationControlData* buf);