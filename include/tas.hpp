#pragma once

extern "C"
{
    #include <switch.h>
    #include <stdio.h>

    extern u32 frame_advance_wait_time_ns;
    extern ViDisplay disp;
    extern Event vsyncEvent;
}

#include <string>
#include <array>

struct TAS
{
    constexpr static int MAX_LINE_LEN = 256;
    FILE* file;
    int line_cnt;
    int frame_counter;
    std::array<int, 6> nxt_line;
    bool is_paused;

    TAS();
    ~TAS();

    void load(std::string file_name, bool auto_run);
    bool readLine();
    void kill();
    bool isActive();
};

std::array<int, 6> parseNxTasStr(char *str);
void attach();
void detach();
bool getIsPaused();
void advanceFrames(const int cnt);