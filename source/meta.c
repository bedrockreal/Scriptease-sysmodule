#include "meta.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

u64 getPID()
{
    u64 pid = 0;
    R_DEBUG(pmdmntGetApplicationProcessId(&pid));
    return pid;
}

u64 getMainNsoBase(u64 pid){
    LoaderModuleInfo proc_modules[2];
    s32 numModules = 0;
    R_DEBUG(ldrDmntGetProcessModuleInfo(pid, proc_modules, 2, &numModules));

    LoaderModuleInfo *proc_module = 0;
    if(numModules == 2){
        proc_module = &proc_modules[1];
    }else{
        proc_module = &proc_modules[0];
    }
    return proc_module->base_address;
}

u64 getHeapBase(Handle handle){
    u64 heap_base = 0;
    R_DEBUG(svcGetInfo(&heap_base, InfoType_HeapRegionAddress, debughandle, 0));
    return heap_base;
}

u64 getTitleId(u64 pid){
    u64 titleId = 0;
    R_DEBUG(pminfoGetProgramId(&titleId, pid));
    return titleId;
}

u64 getTitleVersion(u64 pid){
    u64 titleV = 0;
    s32 out;

    R_ASSERT(nsInitialize());

    NsApplicationContentMetaStatus *MetaStatus = malloc(sizeof(NsApplicationContentMetaStatus[100U]));
    R_DEBUG(nsListApplicationContentMetaStatus(getTitleId(pid), 0, MetaStatus, 100, &out));
    for (int i = 0; i < out; i++) {
        if (titleV < MetaStatus[i].version) titleV = MetaStatus[i].version;
    }

    free(MetaStatus);
    nsExit();

    return (titleV / 0x10000);
}

void getBuildID(u8* out, u64 pid){
    // build id must be 0x20 bytes
    LoaderModuleInfo proc_modules[2];
    s32 numModules = 0;
    R_DEBUG(ldrDmntGetProcessModuleInfo(pid, proc_modules, 2, &numModules));

    LoaderModuleInfo *proc_module = 0;
    if(numModules == 2){
        proc_module = &proc_modules[1];
    }else{
        proc_module = &proc_modules[0];
    }
    memcpy(out, proc_module->build_id, 0x20);
}

u64 getoutsize(NsApplicationControlData* buf){
    R_ASSERT(nsInitialize());
    u64 outsize = 0;
    u64 pid = getPID();
    R_DEBUG(nsGetApplicationControlData(NsApplicationControlSource_Storage, getTitleId(pid), buf, sizeof(NsApplicationControlData), &outsize));
    nsExit();
    return outsize;
}