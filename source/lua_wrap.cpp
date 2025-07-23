#include "lua_wrap.hpp"

#include <vector>
#include <string>

extern "C"
{
    #include <switch.h>
    #include "commands.h"
    #include "common.h"
    #include "freeze.h"
    #include <util.h>
}

std::vector<u8> lua_peek(u64 offset, u64 size)
{
    u8* out = static_cast<u8*>(malloc(size));
    readMem(out, offset, size);

    std::vector<u8> ret(size);
    for (int i = 0; i < size; ++i) ret[i] = out[i];
    free(out);
    return ret;
}

void lua_poke(u64 offset, u64 size, sol::as_table_t<std::vector<unsigned char> > val)
{
    u8* in = static_cast<u8*>(malloc(size));
    for (int i = 0 ; i < size; ++i) in[i] = val.value()[i];
    writeMem(offset, size, in);
    free(in);
}

std::vector<u8> lua_getBuildID(u64 pid)
{
    u8* out = static_cast<u8*>(malloc(0x20));
    getBuildID(out, pid);

    std::vector<u8> ret(0x20);
    for (int i = 0; i < 0x20; ++i) ret[i] = out[i];
    free(out);
    return ret;
}

void lua_clearFreezes()
{
    clearFreezes();
	freeze_thr_state = Idle;
}

void lua_pauseFreezes() { freeze_thr_state = Pause; }
void lua_unpauseFreezes() { freeze_thr_state = Active; }

void lua_loadTAS(std::string arg) { loadTAS(arg.c_str()); }

std::vector<u8> lua_capScreen()
{
    //errors with 0x668CE, unless debugunit flag is patched
    // u64 bSize = 0x7D000;
    // u8* buf = static_cast<u8*>(malloc(bSize));
    // u64 outSize = 0;
    // Result rc = capsscCaptureForDebug(buf, bSize, &outSize);
    // if (R_FAILED(rc))
    //     printf("capssc, 1204: %d\n", rc);

    size_t buf_size = 0x80000;
    u8* buf = (u8*)malloc(buf_size);
    u64 out_size;
    Result rc = capsscCaptureJpegScreenShot(&out_size, buf, buf_size, ViLayerStack_Default, 2e9);
    if (R_FAILED(rc))
        printf("capsscCaptureJpegScreenShot: %d\n", rc);

    std::vector<u8> ret(out_size);
    for (u64 i = 0; i < out_size; ++i) ret[i] = buf[i];
    free(buf);
    return ret;
}

void lua_key(sol::as_table_t<std::vector<unsigned char> > key_seq, bool multi)
{
    int count = key_seq.value().size();
    HiddbgKeyboardAutoPilotState* keystates = static_cast<HiddbgKeyboardAutoPilotState*>(calloc(count, sizeof (HiddbgKeyboardAutoPilotState)));
    for (int i = 0; i < count; ++i)
    {
        u8 key = key_seq.value()[i];
        if (key < 4 || key > 231) continue;
        keystates[i].keys[key / 64] = 1UL << key;
        keystates[i].modifiers = 1024UL; //numlock
    }

    makeKeys(keystates, (multi ? 1 : count));
}

void lua_setControllerType(u8 type)
{
    detachController();
    controllerInitializedType = static_cast<HidDeviceType>(type);
}

void lua_setClickWait(u64 clickWait) { buttonClickSleepTime = clickWait; }
u64 lua_getClickWait() { return buttonClickSleepTime; }

void lua_setPollUpd(u64 pollUpd) { pollRate = pollUpd; }
u64 lua_getPollUpd() { return pollRate; }

void lua_setAdvWait(u64 advWait) { frameAdvanceWaitTimeNs = advWait; }
u64 lua_getAdvWait() { return frameAdvanceWaitTimeNs; }

void lua_setKeyWait(u64 keyWait) { keyPressSleepTime = keyWait; }
u64 lua_getKeyWait() { return keyPressSleepTime; }

void lua_setFreezeUpd(u64 freezeUpd) { freezeRate = freezeUpd; }
u64 lua_getFreezeUpd() { return freezeRate; }

void lua_setFingerDiameter(u32 d) { fingerDiameter = d; }
u32 lua_getFingerDiameter() { return fingerDiameter; }

int lua_getFDCount() { return fd_count; }

void luaInit(sol::state& lua)
{
    lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::table, sol::lib::io, sol::lib::string, sol::lib::math);

    // register types

    // register functions

    // remote control
    lua.set_function("click", &click);
    lua.set_function("press", &press);
    lua.set_function("release", &release);

    // controller
    lua.set_function("attach", &attach);
    lua.set_function("detach", &detach);
    lua.set_function("detachController", &detachController);
    lua.set_function("setControllerType", &lua_setControllerType);

    // memory
    lua.set_function("peek", &lua_peek);
    lua.set_function("poke", &lua_poke);
    lua.set_function("getMainNsoBase", &getMainNsoBase);
    lua.set_function("getHeapBase", &getHeapBase);

    // TAS
    lua.set_function("advTAS", &advanceFrames);
    lua.set_function("setControllerState", &setControllerState);
    lua.set_function("resetControllerState", &resetControllerState);
    // lua.set_function("followMainPointer", &followMainPointer);
    lua.set_function("getIsPaused", &getIsPaused);
    lua.set_function("loadTAS", &lua_loadTAS);
    lua.set_function("cancelTAS", &cancelTAS);

    // utilities
    lua.set_function("getPID", &getPID);
    lua.set_function("getTID", &getTitleId);
    lua.set_function("GetTitleVer", &getTitleVersion);
    lua.set_function("getBuildID", &lua_getBuildID);
    lua.set_function("getIsProgramOpen", &getIsProgramOpen);
    lua.set_function("capScreen", &lua_capScreen);
    lua.set_function("screenOff", &screenOff);
    lua.set_function("screenOn", &screenOn);
    lua.set_function("batteryPercent", &getBatteryChargePercentage);

    // freeze
    lua.set_function("freezeHD", &freezeShort);
    lua.set_function("freezeD", &freezeInt);
    lua.set_function("freezeLL", &freezeLongLong);
    lua.set_function("freezeF", &freezeFloat);
    lua.set_function("freezeLF", &freezeDouble);
    lua.set_function("freezeLLF", &freezeLongDouble);
    lua.set_function("unfreeze", &removeFromFreezeMap);
    lua.set_function("getFreezeCount", &getFreezeCount);
    lua.set_function("clearFreezes", &lua_clearFreezes);
    lua.set_function("pauseFreezes", &lua_pauseFreezes);
    lua.set_function("unpauseFreezes", &lua_unpauseFreezes);

    // keyboard
    lua.set_function("key", &lua_key);

    // touch
    lua.set_function("cancelTouch", &cancelTouch);

    // params
    lua.set_function("setClickWait", &lua_setClickWait);
    lua.set_function("getClickWait", &lua_getClickWait);
    lua.set_function("setAdvWait", &lua_setAdvWait);
    lua.set_function("getAdvWait", &lua_getAdvWait);
    lua.set_function("setKeyWait", &lua_setKeyWait);
    lua.set_function("getKeyWait", &lua_getKeyWait);
    lua.set_function("setFingerDiameter", &lua_setFingerDiameter);
    lua.set_function("getFingerDiameter", &lua_getFingerDiameter);
    lua.set_function("setPollUpd", &lua_setPollUpd);
    lua.set_function("getPollUpd", &lua_getPollUpd);
    lua.set_function("setFreezeUpd", &lua_setFreezeUpd);
    lua.set_function("getFreezeUpd", &lua_getFreezeUpd);
    lua.set_function("getFDCount", &lua_getFDCount);

    // buttons
    for (int i = 0; i < 16; ++i)
    {
        lua.set(translate_keys[i], (1 << i));
    }
    lua.set("KEY_HOME", HiddbgNpadButton_Home);
    lua.set("KEY_CAPTURE", HiddbgNpadButton_Capture);
}