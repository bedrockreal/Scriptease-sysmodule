extern "C"
{
    #include <switch.h>
}

#include <vector>
#include <string>
#include "tas.hpp"

struct bot
{
    static u64 button_click_sleep_time;

    bot();
    bot(int _type);
    ~bot();

    void reconnect();
    
    void click(HidNpadButton btn);
    void press(HidNpadButton btn);
    void release(HidNpadButton btn);
    
    void setState(int btn_state, int lx, int ly, int rx, int ry);
    void resetState();

    void loadScript(std::string filename, bool auto_run);
    void stopScript();
    void resumeScript();
    void updateScript();
    void killScript();

    u8 getType();
    u64 getHandle();
    static u64 getSessionId();
    static void init();
    static void exit();

    private:
    HiddbgHdlsHandle handle;
    HiddbgHdlsDeviceInfo device;
    HiddbgHdlsState state;
    TAS* tas;

    constexpr static int JOYSTICK_LEFT = 0;
    constexpr static int JOYSTICK_RIGHT = 1;
    static u8* workmem;
    static size_t workmem_size;
    static HiddbgHdlsSessionId session_id;

    void setStickState(int side, int x, int y);
    void applyState();

    void selfAttach(int _type);
    void selfDetach();
};

extern std::vector<bot> bots;