extern "C"
{
    #include <switch.h>
}

#include <vector>
#include <string>
#include "tas.hpp"

struct bot
{
    static int button_click_sleep_time;
    static size_t workmem_size;
    constexpr static int JOYSTICK_LEFT = 0;
    constexpr static int JOYSTICK_RIGHT = 1;

    TAS tas;

    bot();
    bot(int _type);
    ~bot();
    
    void click(HidNpadButton btn);
    void press(HidNpadButton btn);
    void release(HidNpadButton btn);
    
    void setState(int btn_state, int lx, int ly, int rx, int ry);
    void resetState();
    void stopScript(bool kill);
    void updateScript();

    u8 getType();
    u64 getHandle();
    u64 getSessionId();

    private:
    HiddbgHdlsHandle handle;
    HiddbgHdlsDeviceInfo device;
    HiddbgHdlsState state;
    u8* workmem;
    static HiddbgHdlsSessionId session_id;

    void setStickState(int side, int x, int y);
};

extern std::vector<bot> bots;