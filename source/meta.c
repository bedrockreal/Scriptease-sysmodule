#include "meta.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

u64 getPID()
{
    u64 pid = 0;    
    Result rc = pmdmntGetApplicationProcessId(&pid);
    if (R_FAILED(rc))
        printf("pmdmntGetApplicationProcessId: %d\n", rc);
    return pid;
}

u64 getMainNsoBase(u64 pid){
    LoaderModuleInfo proc_modules[2];
    s32 numModules = 0;
    Result rc = ldrDmntGetProcessModuleInfo(pid, proc_modules, 2, &numModules);
    if (R_FAILED(rc))
        printf("ldrDmntGetProcessModuleInfo: %d\n", rc);

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
    Result rc = svcGetInfo(&heap_base, InfoType_HeapRegionAddress, debughandle, 0);
    if (R_FAILED(rc))
        printf("svcGetInfo: %d\n", rc);

    return heap_base;
}

u64 getTitleId(u64 pid){
    u64 titleId = 0;
    Result rc = pminfoGetProgramId(&titleId, pid);
    if (R_FAILED(rc))
        printf("pminfoGetProgramId: %d\n", rc);
    return titleId;
}

u64 getTitleVersion(u64 pid){
    u64 titleV = 0;
    s32 out;

    Result rc = nsInitialize();
	if (R_FAILED(rc)) 
        fatalThrow(rc);

    NsApplicationContentMetaStatus *MetaStatus = malloc(sizeof(NsApplicationContentMetaStatus[100U]));
    rc = nsListApplicationContentMetaStatus(getTitleId(pid), 0, MetaStatus, 100, &out);
    if (R_FAILED(rc))
        printf("nsListApplicationContentMetaStatus: %d\n", rc);
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
    Result rc = ldrDmntGetProcessModuleInfo(pid, proc_modules, 2, &numModules);
    if (R_FAILED(rc))
        printf("ldrDmntGetProcessModuleInfo: %d\n", rc);

    LoaderModuleInfo *proc_module = 0;
    if(numModules == 2){
        proc_module = &proc_modules[1];
    }else{
        proc_module = &proc_modules[0];
    }
    memcpy(out, proc_module->build_id, 0x20);
}

u64 getoutsize(NsApplicationControlData* buf){
    Result rc = nsInitialize();
    if (R_FAILED(rc))
        fatalThrow(rc);
    u64 outsize = 0;
    u64 pid = getPID();
    rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, getTitleId(pid), buf, sizeof(NsApplicationControlData), &outsize);
    if (R_FAILED(rc)) {
        printf("nsGetApplicationControlData() failed: 0x%x\n", rc);
    }
    nsExit();
    return outsize;
}