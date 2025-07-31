#include "control.hpp"

extern "C" {
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>

#include "misc.h"
#include "util.h"
#include "freeze.h"
#include "common.h"
#include "meta.h"
#include "mem.h"

#define TITLE_ID 0x430000000000000D
#define HEAP_SIZE 0x00400000
#define THREAD_SIZE 0x1A000
#define VERSION_S "2.4"

Thread freezeThread, touchThread;

// locks for thread
Mutex freezeMutex, touchMutex;

// events for releasing or idling threads
FreezeThreadState freeze_thr_state = Active; 
// key and touch events currently being processed
TouchData currentTouchEvent = {0};

// for cancelling the touch/click thread
u8 touchToken = 0;

// fd counters and max size
int fd_count = 0;
int fd_size = 5;

// we aren't an applet
u32 __nx_applet_type = AppletType_None;

u64 freezeRate = 3;

struct pollfd *pfds;

ViDisplay disp;
Event vsyncEvent;

// we override libnx internals to do a minimal init
void __libnx_initheap(void)
{
	static u8 inner_heap[HEAP_SIZE];
    extern void* fake_heap_start;
    extern void* fake_heap_end;

    // Configure the newlib heap.
    fake_heap_start = inner_heap;
    fake_heap_end   = inner_heap + sizeof(inner_heap);
}

void __appInit(void)
{
    Result rc;
    svcSleepThread(20000000000L);
    R_ASSERT(smInitialize());

    if (hosversionGet() == 0) {
        rc = setsysInitialize();
        if (R_SUCCEEDED(rc)) {
            SetSysFirmwareVersion fw;
            rc = setsysGetFirmwareVersion(&fw);
            if (R_SUCCEEDED(rc))
                hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
            setsysExit();
        }
    }

    R_ASSERT(pmdmntInitialize());
    R_ASSERT(ldrDmntInitialize());
    R_ASSERT(pminfoInitialize());
    R_ASSERT(socketInitializeDefault());
    R_ASSERT(viInitialize(ViServiceType_System));
    R_ASSERT(fsInitialize());
    R_ASSERT(fsdevMountSdmc());
    R_ASSERT(hiddbgInitialize());
}

void __attribute__((weak)) userAppExit(void);

void __appExit(void)
{
    // if (R_SUCCEEDED(rc))
    {
	    freeze_thr_state = Exit;
		threadWaitForExit(&freezeThread);
        threadClose(&freezeThread);

        currentTouchEvent.state = 3;
        threadWaitForExit(&touchThread);
        threadClose(&touchThread);
	}
	
	clearFreezes();
    freeFreezes();

    smExit();
    nsExit();
    audoutExit();
    socketExit();

    eventClose(&vsyncEvent);
    viCloseDisplay(&disp);
    viExit();

    fsExit();
    fsdevUnmountAll();
    hiddbgExit();
}

void sub_freeze(void *arg);
void sub_touch(void *arg);
}

#include "lua_wrap.hpp"

std::vector<bot> bots;

int main()
{
    R_ASSERT(viOpenDefaultDisplay(&disp));
    R_ASSERT(viGetDisplayVsyncEvent(&disp, &vsyncEvent));
    
    char *linebuf = static_cast<char*>(malloc(sizeof(char) * MAX_LINE_LENGTH));
    pfds = static_cast<pollfd*>(malloc(sizeof *pfds * fd_size));

    int c = sizeof(struct sockaddr_in);
    struct sockaddr_in client;

    int listenfd = setupServerSocket(6000);
    pfds[0].fd = listenfd;
    pfds[0].events = POLLIN;
    fd_count = 1;

    int clientfd;
	int fr_count = 0;
	
    initFreezes();

	// freeze thread
	mutexInit(&freezeMutex);
	Result rc = threadCreate(&freezeThread, sub_freeze, (void*)&freeze_thr_state, NULL, THREAD_SIZE, 0x2C, -2); 
	if (R_SUCCEEDED(rc))
        rc = threadStart(&freezeThread);

    // touch thread
    mutexInit(&touchMutex);
    rc = threadCreate(&touchThread, sub_touch, (void*)&currentTouchEvent, NULL, THREAD_SIZE, 0x2C, -2); 
    if (R_SUCCEEDED(rc))
        rc = threadStart(&touchThread);

    sol::state lua;
    luaInit(lua);
    flashLed();

    while (true)
    {
        int poll_upds = poll(pfds, fd_count, 10);
        if (poll_upds > 0)
        {
            mutexLock(&freezeMutex);
            for(int i = 0; i < fd_count; i++) 
            {
                if (pfds[i].revents & POLLIN) 
                {
                    if (pfds[i].fd == listenfd) 
                    {
                        clientfd = accept(listenfd, (struct sockaddr *)&client, (socklen_t *)&c);
                        if(clientfd != -1)
                        {
                            add_to_pfds(&pfds, clientfd, &fd_count, &fd_size);
                        }
                        else{
                            svcSleepThread(1e+9L);
                            close(listenfd);
                            listenfd = setupServerSocket(6000);
                            pfds[0].fd = listenfd;
                            pfds[0].events = POLLIN;
                            break;
                        }
                    }
                    else
                    {
                        bool readEnd = false;
                        int readBytesSoFar = 0;
                        while(!readEnd){
                            int len = read(pfds[i].fd, &linebuf[readBytesSoFar], 1);
                            if(len <= 0)
                            {
                                // end of input
                                close(pfds[i].fd);
                                del_from_pfds(pfds, i, &fd_count);
                                readEnd = true;
                            }
                            else
                            {
                                readBytesSoFar += len;
                                if(linebuf[readBytesSoFar-1] == '\n'){
                                    // end of line
                                    if (pfds[i].fd == clientfd)
                                    {
                                        readEnd = true;
                                        dup2(pfds[i].fd, STDOUT_FILENO);

                                        // u64 start_ns = armTicksToNs(armGetSystemTick());
                                        try
                                        {
                                            if (linebuf[0] == '-' && linebuf[1] == '-')
                                            {
                                                linebuf[readBytesSoFar - 1] = 0;
                                                int offset = 2;
                                                for (; offset < readBytesSoFar && linebuf[offset] == ' '; ++offset);
                                                lua.safe_script_file(std::string(linebuf + offset));
                                            }
                                            else lua.safe_script(linebuf);
                                        }
                                        catch(const sol::error& e)
                                        {
                                            printf("%s\n", e.what());
                                        }
                                        // u64 end_ns = armTicksToNs(armGetSystemTick());
                                        // printf("%.6lf\n", (end_ns - start_ns) / 1e6);

                                        printf("%lu\n", bots.size());

                                        fflush(stdout);
                                        memset(linebuf, 0, readBytesSoFar);
                                    }
                                    else { }
                                }
                            }
                        }
                    }
                }
            }
        }

		if (!getIsPaused())
		{
			for (int i = 0; i < bots.size(); ++i) bots[i].updateScript();
		}

        fr_count = getFreezeCount();
        if (fr_count == 0)
            freeze_thr_state = Idle;
        mutexUnlock(&freezeMutex);
		
        R_ASSERT(eventWait(&vsyncEvent, 0xFFFFFFFFFFF));
    }
	
    bots.clear();
    return 0;
}

void sub_freeze(void *arg)
{
	u64 heap_base;
	u64 tid_now = 0;
	u64 pid = 0;
	bool wait_su = false;
	int freezecount = 0;
	
	IDLE:
    while (freezecount == 0)
	{
		if (*(FreezeThreadState*)arg == Exit)
			break;
		
		// do nothing
		svcSleepThread(1e+9L);
		freezecount = getFreezeCount();
	}
	
	while (1)
	{
		if (*(FreezeThreadState*)arg == Exit)
			break;
		
		if (*(FreezeThreadState*)arg == Idle) // no freeze
		{
			mutexLock(&freezeMutex);
			freeze_thr_state = Active;
			mutexUnlock(&freezeMutex); // stupid but it works so is it really stupid? (yes)
			freezecount = 0;
			wait_su = false;
			goto IDLE;
		}
        else if (*(FreezeThreadState*)arg == Pause)
        {
            svcSleepThread(1e+8L); //1s
            continue;
        }
		
		mutexLock(&freezeMutex);
		attach();
		heap_base = getHeapBase(debughandle);
		pmdmntGetApplicationProcessId(&pid);
		tid_now = getTitleId(pid);
		detach();
		
		// don't freeze on startup of new tid to remove any chance of save corruption
		if (tid_now == 0)
		{
			mutexUnlock(&freezeMutex);
			svcSleepThread(1e+10L);
			wait_su = true;
			continue;
		}
		
		if (wait_su)
		{
			mutexUnlock(&freezeMutex);
			svcSleepThread(3e+10L);
			wait_su = false;
			mutexLock(&freezeMutex);
		}
		
		if (heap_base > 0)
		{
			attach();
			for (int j = 0; j < FREEZE_DIC_LENGTH; j++)
			{
				if (freezes[j].state == 1 && freezes[j].titleId == tid_now)
				{
					writeMem(heap_base + freezes[j].address, freezes[j].size, freezes[j].vData);
				}
			}
			detach();
		}
		
		mutexUnlock(&freezeMutex);
		svcSleepThread(freezeRate * 1e+6L);
		tid_now = 0;
		pid = 0;
	}
}

void sub_touch(void *arg)
{
    while (1)
    {
        TouchData* touchPtr = (TouchData*)arg;
        if (touchPtr->state == 1)
        {
            mutexLock(&touchMutex); // don't allow any more assignments to the touch var (will lock the main thread)
            touchMulti(touchPtr->states, touchPtr->sequentialCount, touchPtr->holdTime, touchPtr->hold, &touchToken);
            free(touchPtr->states);
            touchPtr->state = 0;
            mutexUnlock(&touchMutex);
        }

        svcSleepThread(1e+6L);
        
        touchToken = 0;

        if (touchPtr->state == 3)
            break;
    }
}
