#include "control.hpp"
#include "util.h"

extern "C"
{
    #include <stdio.h>
    #include <stdlib.h>
}

bot::bot() : bot(3) {}

bot::bot(int _type) {selfAttach(_type); }

bot::~bot() { selfDetach(); }

void bot::click(HidNpadButton btn)
{
    press(btn);
    svcSleepThread(bot::button_click_sleep_time * 1e+6L);
    release(btn);
}

void bot::press(HidNpadButton btn)
{
    state.buttons |= btn;
    applyState();
}

void bot::release(HidNpadButton btn)
{
    state.buttons &= ~btn;
    applyState();
}

void bot::setStickState(int side, int x, int y)
{
    // check oob
    if (abs(x) > 0x7fff || abs(y) > 0x7fff) return;
    if (x * x + y * y > 0x7fff * 0x7fff) return;
    
    if (side == JOYSTICK_LEFT)
    {	
        state.analog_stick_l.x = x;
		state.analog_stick_l.y = y;
	}
	else
	{
		state.analog_stick_r.x = x;
		state.analog_stick_r.y = y;
	}
}

void bot::setState(int btn_state, int lx, int ly, int rx, int ry)
{
    state.buttons = HidNpadButton(btn_state);
    setStickState(JOYSTICK_LEFT, lx, ly);
    setStickState(JOYSTICK_RIGHT, rx, ry);

    applyState();
}

void bot::resetState() { setState(0, 0, 0, 0, 0); }

void bot::applyState()
{
    R_DEBUG(hiddbgSetHdlsState(handle, &state));
}

void bot::loadScript(std::string filename, bool auto_run)
{
    if (tas) killScript();
    tas = new TAS();
    tas->load(filename, auto_run);
}

void bot::stopScript() { tas->is_paused = 1; }

void bot::resumeScript() { tas->is_paused = 0; }

void bot::updateScript()
{
    if (tas == NULL || tas->is_paused) return;
    if (!tas->isActive()) killScript();
    else if (tas->frame_counter++ == tas->nxt_line[0])
    {
        auto& tmp = tas->nxt_line;
        setState(tmp[1], tmp[2], tmp[3], tmp[4], tmp[5]);
        if (tas->readLine()) killScript();
    }
}

u8 bot::getType() { return u8(device.deviceType); }
u64 bot::getHandle() { return handle.handle; }
u64 bot::getSessionId() { return session_id.id; }

void bot::init()
{
    R_ASSERT(hiddbgInitialize());
    workmem = static_cast<u8*>(aligned_alloc(0x1000, workmem_size));
    if (!workmem) printf("workmem alloc failed\n");

    R_DEBUG(hiddbgAttachHdlsWorkBuffer(&session_id, workmem, workmem_size));
}

void bot::exit()
{
    R_DEBUG(hiddbgReleaseHdlsWorkBuffer(session_id));
}

void bot::killScript()
{
    tas->kill();
    delete tas;
    tas = NULL;
}

void bot::selfAttach(int _type)
{
    // Set the bot type to Pro-Controller, and set the npadInterfaceType.
    device.deviceType = HidDeviceType(_type); // default to Pro Controller
    device.npadInterfaceType = HidNpadInterfaceType_Bluetooth;
    // Set the controller colors. The grip colors are for Pro-Controller on [9.0.0+].
    device.singleColorBody = RGBA8_MAXALPHA(255,255,255);
    device.singleColorButtons = RGBA8_MAXALPHA(0,0,0);
    device.colorLeftGrip = RGBA8_MAXALPHA(230,255,0);
    device.colorRightGrip = RGBA8_MAXALPHA(0,40,20);

    R_DEBUG(hiddbgAttachHdlsVirtualDevice(&handle, &device));
    
    // Setup example controller state.
    state.battery_level = 4; // Set battery charge to full.
    resetState();

    tas = NULL;
}

void bot::selfDetach()
{
    if (tas) killScript();
    R_DEBUG(hiddbgDetachHdlsVirtualDevice(handle));
}

void bot::reconnect()
{
    int type = getType();
    selfDetach();
    selfAttach(type);
}

u64 bot::button_click_sleep_time = 50;
size_t bot::workmem_size = 0x1000;
u8* bot::workmem = NULL;
HiddbgHdlsSessionId bot::session_id = {
    .id = 0,
};