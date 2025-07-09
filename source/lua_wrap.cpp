#include "lua_wrap.hpp"

#include <vector>

extern "C"
{
    #include <switch.h>
    #include "commands.h"
    #include "common.h"
    #include "freeze.h"
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

void luaInit(sol::state& lua)
{
    // adopted from tas-script
    lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::table, sol::lib::io, sol::lib::string, sol::lib::math);

    // Register Lua functions
    // registerSVC(lua);
    // registerHID(lua);
    // registerHIDDBG(lua);
    // registerVI(lua);
    // registerSwitch(lua);

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

    // memory
    lua.set_function("peek", &lua_peek);
    lua.set_function("poke", &lua_poke);
    lua.set_function("getMainNsoBase", &getMainNsoBase);
    lua.set_function("getHeapBase", &getHeapBase);

    // TAS
    lua.set_function("advanceFrames", &advanceFrames);
    lua.set_function("setControllerState", &setControllerState);
    lua.set_function("resetControllerState", &resetControllerState);
    // lua.set_function("followMainPointer", &followMainPointer);
    lua.set_function("getIsPaused", &getIsPaused);
    lua.set_function("loadTAS", &loadTAS);
    lua.set_function("cancelTAS", &cancelTAS);

    // utilities
    lua.set_function("getPID", &getPID);
    lua.set_function("getTitleId", &getTitleId);
    lua.set_function("GetTitleVersion", &GetTitleVersion);
    lua.set_function("getBuildID", &lua_getBuildID);
    lua.set_function("getIsProgramOpen", &getIsProgramOpen);
    lua.set_function("screenOff", &screenOff);
    lua.set_function("screenOn", &screenOn);

    // freeze
    lua.set_function("freezeShort", &freezeShort);
    lua.set_function("freezeInt", &freezeInt);
    lua.set_function("freezeLongLong", &freezeLongLong);
    lua.set_function("freezeFloat", &freezeFloat);
    lua.set_function("freezeDouble", &freezeDouble);
    lua.set_function("freezeLongDouble", &freezeLongDouble);
    lua.set_function("unfreeze", &removeFromFreezeMap);
    lua.set_function("getFreezeCount", &getFreezeCount);
    lua.set_function("clearFreezes", &lua_clearFreezes);
    lua.set_function("pauseFreezes", &lua_pauseFreezes);
    lua.set_function("unpauseFreezes", &lua_unpauseFreezes);

    // buttons
    for (int i = 0; i < 16; ++i)
    {
        lua.set(translate_keys[i], (1 << i));
    }
    lua.set("KEY_HOME", HiddbgNpadButton_Home);
    lua.set("KEY_CAPTURE", HiddbgNpadButton_Capture);
}