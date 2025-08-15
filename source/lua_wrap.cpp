#define SOL_ALL_SAFETIES_ON 1

#include "lua_wrap.hpp"
#include "control.hpp"

#include <vector>
#include <string>

extern "C"
{
    #include <switch.h>
    #include "common.h"
    #include "freeze.h"
    #include "meta.h"
    #include "mem.h"
    #include "util.h"

    extern const char* const translate_keys[16];
}

extern FreezeThreadState freeze_thr_state;

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
    u64 out_size = 0;
    R_DEBUG(capsscCaptureJpegScreenShot(&out_size, &buf[0], buf_size, ViLayerStack_Default, 2e9));

    buf.resize(out_size);
    capsscExit();
    return buf;
}

void lua_setClickWait(u64 clickWait) { bot::button_click_sleep_time = clickWait; }
u64 lua_getClickWait() { return bot::button_click_sleep_time; }

void lua_setAdvWait(u64 advWait) { frame_advance_wait_time_ns = advWait; }
u64 lua_getAdvWait() { return frame_advance_wait_time_ns; }

void lua_setFreezeUpd(u64 freezeUpd) { freeze_rate = freezeUpd; }
u64 lua_getFreezeUpd() { return freeze_rate; }

int lua_getFDCount() { return fd_count; }

template <class T>
std::vector<u8> lua_getBytes(T x)
{
    u8 size = sizeof(x);
    std::vector<u8> buf(size);
    memcpy(&buf[0], &x, size);
    return buf;
}

void luaInit(sol::state& lua)
{
    lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::table, sol::lib::io, sol::lib::string, sol::lib::math);

    // register types
    // call new() using dots, colons otherwise; will crash otherwise
    // sol::usertype tas_type = lua.new_usertype<TAS>(
    //     "TAS", sol::constructors<TAS()>(),
    //     "is_paused", &TAS::is_paused,
    //     "isActive", &TAS::isActive,
    //     "load", &TAS::load
    // );

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

    bot_type["loadScript"] = &bot::loadScript;
    bot_type["stopScript"] = &bot::stopScript;
    bot_type["resumeScript"] = &bot::resumeScript;
    bot_type["killScript"] = &bot::killScript;
    bot_type["reconnect"] = &bot::reconnect;

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

    lua.set_function("getBytesShort", &lua_getBytes<s16>);
    lua.set_function("getBytesInt", &lua_getBytes<s32>);
    lua.set_function("getBytesLongLong", &lua_getBytes<s64>);
    lua.set_function("getBytesFloat", &lua_getBytes<float>);
    lua.set_function("getBytesDouble", &lua_getBytes<double>);
    lua.set_function("getBytesLongDouble", &lua_getBytes<long double>);

    // freeze
    lua.set_function("unfreeze", &removeFromFreezeMap);
    lua.set_function("getFreezeCount", &getFreezeCount);
    lua.set_function("clearFreezes", &lua_clearFreezes);
    lua.set_function("pauseFreezes", &lua_pauseFreezes);
    lua.set_function("unpauseFreezes", &lua_unpauseFreezes);

    // params
    lua.set_function("setClickWait", &lua_setClickWait);
    lua.set_function("getClickWait", &lua_getClickWait);
    lua.set_function("setAdvWait", &lua_setAdvWait);
    lua.set_function("getAdvWait", &lua_getAdvWait);
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