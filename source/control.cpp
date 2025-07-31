#include "control.hpp"

extern "C"
{
    #include <stdio.h>
    #include <stdlib.h>
}

bot::bot() { bot(3); }

bot::bot(int _type)
{
    printf("bot(int)\n");
    if (getSessionId() == 0) {
        workmem = static_cast<u8*>(aligned_alloc(0x1000, workmem_size));
        if (!workmem) printf("workmem alloc failed\n");
    }
	
	// Set the bot type to Pro-Controller, and set the npadInterfaceType.
    device.deviceType = HidDeviceType(_type); // default to Pro Controller
    device.npadInterfaceType = HidNpadInterfaceType_Bluetooth;
    // Set the controller colors. The grip colors are for Pro-Controller on [9.0.0+].
    device.singleColorBody = RGBA8_MAXALPHA(255,255,255);
    device.singleColorButtons = RGBA8_MAXALPHA(0,0,0);
    device.colorLeftGrip = RGBA8_MAXALPHA(230,255,0);
    device.colorRightGrip = RGBA8_MAXALPHA(0,40,20);

    // Setup example controller state.
    state.battery_level = 4; // Set battery charge to full.
    state.analog_stick_l.x = 0x0;
    state.analog_stick_l.y = -0x0;
    state.analog_stick_r.x = 0x0;
    state.analog_stick_r.y = -0x0;

    Result rc;
    if (getSessionId() == 0)
    {
        rc = hiddbgAttachHdlsWorkBuffer(&session_id, workmem, workmem_size);
        if (R_FAILED(rc))
            printf("hiddbgAttachHdlsWorkBuffer: 0x%x\n", rc);
    }
    rc = hiddbgAttachHdlsVirtualDevice(&handle, &device);
    if (R_FAILED(rc))
        printf("hiddbgAttachHdlsVirtualDevice: 0x%x\n", rc);
}

bot::~bot()
{
    printf("%lu ~bot()\n", getHandle()); // when add new, always destroy first item (?)

    Result rc = hiddbgDetachHdlsVirtualDevice(handle);
    if (R_FAILED(rc)) printf("hiddbgDetachHdlsVirtualDevice: 0x%x\n", rc);
    rc = hiddbgReleaseHdlsWorkBuffer(session_id);
    if (R_FAILED(rc)) printf("hiddbgReleaseHdlsWorkBuffer: 0x%x\n", rc);
    hiddbgExit();
	free(workmem);
}

void bot::click(HidNpadButton btn)
{
    press(btn);
    svcSleepThread(bot::button_click_sleep_time * 1e+6L);
    release(btn);
}

void bot::press(HidNpadButton btn)
{
    state.buttons |= btn;
    Result rc = hiddbgSetHdlsState(handle, &state);
    if (R_FAILED(rc))
        printf("hiddbgSetHdlsState: 0x%x\n", rc);
}

void bot::release(HidNpadButton btn)
{
    state.buttons &= ~btn;
    Result rc = hiddbgSetHdlsState(handle, &state);
    if (R_FAILED(rc))
        printf("hiddbgSetHdlsState: 0x%x\n", rc);
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

    Result rc = hiddbgSetHdlsState(handle, &state);
    if (R_FAILED(rc))
        printf("hiddbgSetHdlsState: 0x%x\n", rc);
}

void bot::resetState() { setState(0, 0, 0, 0, 0); }

void bot::stopScript(bool kill)
{

}

void bot::updateScript()
{
    if (tas.is_paused || !tas.isActive()) return;
    if (tas.frame_counter++ == tas.nxt_line[0])
    {
        auto& tmp = tas.nxt_line;
        setState(tmp[1], tmp[2], tmp[3], tmp[4], tmp[5]);
        if (!tas.readLine()) tas.kill();
    }
}

u8 bot::getType() { return u8(device.deviceType); }
u64 bot::getHandle() { return handle.handle; }
u64 bot::getSessionId() { return session_id.id; }

int bot::button_click_sleep_time = 50;
size_t bot::workmem_size = 0x1000;
HiddbgHdlsSessionId bot::session_id = {
    .id = 0,
};