#include "lua_wrap.hpp"
#include "control.hpp"

#include <vector>
#include <string>

extern "C"
{
    #include <switch.h>
    #include "misc.h"
    #include "common.h"
    #include "freeze.h"
    #include "meta.h"
    #include "mem.h"

    extern const char* const translate_keys[16];
}

std::vector<u8> lua_peek(u64 offset, u64 size)
{
    std::vector<u8> ret(size);
    readMem(offset, size, &ret[0]);
    return ret;
}

void lua_poke(u64 offset, u64 size, std::vector<u8> val)
{
    writeMem(offset, size, &val[0]);
}

std::vector<u8> lua_getBuildID(u64 pid)
{
    std::vector<u8> ret(0x20);
    getBuildID(&ret[0], pid);
    return ret;
}

void lua_clearFreezes()
{
    clearFreezes();
	freeze_thr_state = Idle;
}

void lua_pauseFreezes() { freeze_thr_state = Pause; }
void lua_unpauseFreezes() { freeze_thr_state = Active; }

std::vector<u8> lua_capScreen()
{
    Result rc = capsscInitialize();
    if (R_FAILED(rc))
    {
        printf("capsscInitialize(): %d\n", int(rc));
        return std::vector<u8>();
    }
    //errors with 0x668CE, unless debugunit flag is patched
    // u64 bSize = 0x7D000;
    // u8* buf = static_cast<u8*>(malloc(bSize));
    // u64 outSize = 0;
    // rc = capsscCaptureForDebug(buf, bSize, &outSize);
    // if (R_FAILED(rc))
    //     printf("capssc, 1204: %d\n", rc);

    size_t buf_size = 0x80000;
    std::vector<u8> buf(buf_size);
    u64 out_size;
    rc = capsscCaptureJpegScreenShot(&out_size, &buf[0], buf_size, ViLayerStack_Default, 2e9);
    if (R_FAILED(rc))
        printf("capsscCaptureJpegScreenShot: %d\n", rc);

    buf.resize(out_size);
    capsscExit();
    return buf;
}

void lua_setTouchState(std::vector<u32> X, std::vector<u32> Y)
{
    // pass
}

void lua_setClickWait(u64 clickWait) { bot::button_click_sleep_time = clickWait; }
u64 lua_getClickWait() { return bot::button_click_sleep_time; }

void lua_setAdvWait(u64 advWait) { frame_advance_wait_time_ns = advWait; }
u64 lua_getAdvWait() { return frame_advance_wait_time_ns; }

void lua_setFreezeUpd(u64 freezeUpd) { freezeRate = freezeUpd; }
u64 lua_getFreezeUpd() { return freezeRate; }

void lua_setFingerDiameter(u32 d) { fingerDiameter = d; }
u32 lua_getFingerDiameter() { return fingerDiameter; }

int lua_getFDCount() { return fd_count; }

void luaInit(sol::state& lua)
{
    lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::table, sol::lib::io, sol::lib::string, sol::lib::math);

    // register types
    // call new() using dots, colons otherwise; will crash otherwise
    sol::usertype tas_type = lua.new_usertype<TAS>(
        "TAS", sol::constructors<TAS()>()
    );
    tas_type["load"] = &TAS::load;

    sol::usertype bot_type = lua.new_usertype<bot>(
        "bot", sol::constructors<bot(), bot(int)>()
    );
    bot_type["click"] = &bot::click;
    bot_type["press"] = &bot::press;
    bot_type["release"] = &bot::release;
    bot_type["setState"] = &bot::setState;
    bot_type["resetState"] = &bot::resetState;
    bot_type["getType"] = &bot::getType;
    bot_type["getHandle"] = &bot::getHandle;
    bot_type["getSessionId"] = &bot::getSessionId;

    // controller
    lua.set("bots", &bots);

    // memory
    lua.set_function("peek", &lua_peek);
    lua.set_function("poke", &lua_poke);
    lua.set_function("getMainNsoBase", &getMainNsoBase);
    lua.set_function("getHeapBase", &getHeapBase);

    // TAS
    lua.set_function("attach", &attach);
    lua.set_function("detach", &detach);
    lua.set_function("advTAS", &advanceFrames);
    lua.set_function("getIsPaused", &getIsPaused);

    // utilities
    lua.set_function("getPID", &getPID);
    lua.set_function("getTID", &getTitleId);
    lua.set_function("GetTitleVer", &getTitleVersion);
    lua.set_function("getBuildID", &lua_getBuildID);
    lua.set_function("getIsProgramOpen", &getIsProgramOpen);
    lua.set_function("capScreen", &lua_capScreen);

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

    // touch
    lua.set_function("cancelTouch", &cancelTouch);

    // params
    lua.set_function("setClickWait", &lua_setClickWait);
    lua.set_function("getClickWait", &lua_getClickWait);
    lua.set_function("setAdvWait", &lua_setAdvWait);
    lua.set_function("getAdvWait", &lua_getAdvWait);
    lua.set_function("setFingerDiameter", &lua_setFingerDiameter);
    lua.set_function("getFingerDiameter", &lua_getFingerDiameter);
    lua.set_function("setFreezeUpd", &lua_setFreezeUpd);
    lua.set_function("getFreezeUpd", &lua_getFreezeUpd);
    lua.set_function("getFDCount", &lua_getFDCount);

    // lua.set_function("dumpHdls", []()
    // {
    //     HiddbgHdlsStateList state_list;
    //     hiddbgDumpHdlsStates(bots[0].session_id, &state_list);
    //     printf("Total entries: %d\n", state_list.total_entries);
    //     printf("Pad: %u\n", state_list.pad);
    //     for (int i = 0; i < state_list.total_entries; ++i)
    //     {
    //         printf("Controller #%d:\n", i);
    //         printf("Handle: %lu\n", state_list.entries[i].handle.handle);
    //         printf("Type: %d\n", int(state_list.entries[i].device.deviceType));
    //     }
    // });

    // buttons
    for (int i = 0; i < 16; ++i)
    {
        lua.set(translate_keys[i], (1 << i));
    }
    lua.set("KEY_HOME", HiddbgNpadButton_Home);
    lua.set("KEY_CAPTURE", HiddbgNpadButton_Capture);
}