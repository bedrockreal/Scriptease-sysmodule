#ifdef __cplusplus
extern "C" {
#endif

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
#include <poll.h>
#include <errno.h>

#include "commands.h"
#include "args.h"
#include "util.h"
#include "freeze.h"
#include "common.h"

Thread freezeThread, touchThread, keyboardThread, clickThread, tasThread;
FILE* tas_script = NULL;

// locks for thread
Mutex freezeMutex, touchMutex, keyMutex, clickMutex, tasMutex;

// events for releasing or idling threads
FreezeThreadState freeze_thr_state = Active; 
u8 clickThreadState = 0, tasThreadState = 0; // 1 = break thread
// key and touch events currently being processed
KeyData currentKeyEvent = {0};
TouchData currentTouchEvent = {0};
char* currentClick = NULL;

// for cancelling the touch/click thread
u8 touchToken = 0;
u8 clickToken = 0;

// fd counters and max size
int fd_count = 0;
int fd_size = 5;
int tasfilefd = 1000;

// we aren't an applet
u32 __nx_applet_type = AppletType_None;

// Needed for TAS
int prev_frame = 0;
int line_cnt = 0;

u64 mainLoopSleepTime = 50;
u64 freezeRate = 3;
bool debugResultCodes = false;
bool echoCommands = false;

struct pollfd *pfds;

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
    rc = smInitialize();
    if (R_FAILED(rc))
        fatalThrow(rc);
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
    rc = pmdmntInitialize();
	if (R_FAILED(rc)) 
        fatalThrow(rc);
    rc = ldrDmntInitialize();
	if (R_FAILED(rc)) 
		fatalThrow(rc);
    rc = pminfoInitialize();
	if (R_FAILED(rc)) 
		fatalThrow(rc);
    rc = socketInitializeDefault();
    if (R_FAILED(rc))
        fatalThrow(rc);
    rc = capsscInitialize();
    if (R_FAILED(rc))
        fatalThrow(rc);
    rc = viInitialize(ViServiceType_Default);
    if (R_FAILED(rc))
        fatalThrow(rc);

    // adopted from usb-botbase
    rc = fsInitialize();
    if (R_FAILED(rc))
        fatalThrow(rc);

    rc = fsdevMountSdmc();
    if (R_FAILED(rc))
        fatalThrow(rc);
}

void __appExit(void)
{
    smExit();
    nsExit();
    audoutExit();
    socketExit();
    viExit();

    fsExit();
    fsdevUnmountAll();
}

void add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size)
{
    if (*fd_count == *fd_size) {
        *fd_size *= 2;

        #ifdef __cplusplus
        *pfds = static_cast<pollfd*>(realloc(*pfds, sizeof(**pfds) * (*fd_size)));
        #else
        *pfds = realloc(*pfds, sizeof(**pfds) * (*fd_size));
        #endif
    }

    (*pfds)[*fd_count].fd = newfd;
    (*pfds)[*fd_count].events = POLLIN;

    (*fd_count)++;
}

void del_from_pfds(struct pollfd pfds[], int i, int *fd_count)
{
    pfds[i] = pfds[*fd_count-1];

    (*fd_count)--;
}

void makeTouch(HidTouchState* state, u64 sequentialCount, u64 holdTime, bool hold)
{
    mutexLock(&touchMutex);
    memset(&currentTouchEvent, 0, sizeof currentTouchEvent);
    currentTouchEvent.states = state;
    currentTouchEvent.sequentialCount = sequentialCount;
    currentTouchEvent.holdTime = holdTime;
    currentTouchEvent.hold = hold;
    currentTouchEvent.state = 1;
    mutexUnlock(&touchMutex);
}

void makeKeys(HiddbgKeyboardAutoPilotState* states, u64 sequentialCount)
{
    mutexLock(&keyMutex);
    memset(&currentKeyEvent, 0, sizeof currentKeyEvent);
    currentKeyEvent.states = states;
    currentKeyEvent.sequentialCount = sequentialCount;
    currentKeyEvent.state = 1;
    mutexUnlock(&keyMutex);
}

void makeClickSeq(char* seq)
{
    mutexLock(&clickMutex);
    currentClick = seq;
    mutexUnlock(&clickMutex);
}

#ifdef __cplusplus
}

#include "sol/sol.hpp"

// #include "lua_switch.hpp"
// #include "lua_svc.hpp"
// #include "lua_hid.hpp"
// #include "lua_hiddbg.hpp"
// #include "lua_vi.hpp"
#include "lua_wrap.hpp"

#endif

int argmain(int argc, char **argv)
{
    if (argc == 0)
        return 0;

    /*

    //poke <address in hex or dec> <data in hex or dec>
    if (!strcmp(argv[0], "poke"))
    {
        if(argc != 3)
            return 0;
            
        MetaData meta = getMetaData();

        u64 offset = parseStringToInt(argv[1]);
        u64 size = 0;
        u8* data = parseStringToByteBuffer(argv[2], &size);
        poke(meta.heap_base + offset, size, data);
        free(data);
    } 
    
    if (!strcmp(argv[0], "pokeAbsolute"))
    {
        if(argc != 3)
            return 0;

        u64 offset = parseStringToInt(argv[1]);
        u64 size = 0;
        u8* data = parseStringToByteBuffer(argv[2], &size);
        poke(offset, size, data);
        free(data);
    }
        
    if (!strcmp(argv[0], "pokeMain"))
    {
        if(argc != 3)
            return 0;
            
        MetaData meta = getMetaData();

        u64 offset = parseStringToInt(argv[1]);
        u64 size = 0;
        u8* data = parseStringToByteBuffer(argv[2], &size);
        poke(meta.main_nso_base + offset, size, data);
        free(data);
    }
    */

    //clickSeq <sequence> eg clickSeq A,W1000,B,W200,DUP,W500,DD,W350,%5000,1500,W2650,%0,0 (some params don't parse correctly, such as DDOWN so use the alt)
    //syntax: <button>=click, 'W<number>'=wait/sleep thread, '+<button>'=press button, '-<button>'=release button, '%<x axis>,<y axis>'=move L stick <x axis, y axis>, '&<x axis>,<y axis>'=move R stick <x axis, y axis> 
    if (!strcmp(argv[0], "clickSeq"))
    {
        if(argc != 2)
            return 0;
        
        u64 sizeArg = strlen(argv[1]) + 1;
        #ifdef __cplusplus
        char* seqNew = static_cast<char*>(malloc(sizeArg));
        #else
        char* seqNew = malloc(sizeArg);
        #endif

        strcpy(seqNew, argv[1]);
        makeClickSeq(seqNew);
    }

    if (!strcmp(argv[0], "clickCancel"))
        clickToken = 1;

    //detachController
    if(!strcmp(argv[0], "detachController"))
    {
        detachController();
    }
    if (!strcmp(argv[0], "game")) 
    {
        if (argc != 2)
            return 0;
        NsApplicationControlData* buf = (NsApplicationControlData*)malloc(sizeof(NsApplicationControlData));
        u64 outsize = getoutsize(buf);
        NacpLanguageEntry* langentry = NULL;    
        if (outsize != 0) {
            if (!strcmp(argv[1], "icon")) {
                u64 i;
                for (i = 0; i < outsize - sizeof(buf->nacp); i++)
                {
                    printf("%02X", buf->icon[i]);
                }
                printf("\n");
            }
            if (!strcmp(argv[1], "version"))
            {
                char version[0x11];
                memset(version, 0, sizeof(version));
                strncpy(version, buf->nacp.display_version, sizeof(version));
                printf("%s\n", version);
            }
            if (!strcmp(argv[1], "rating"))
            {
                printf("%d\n", buf->nacp.rating_age[0]);
            }
            if (!strcmp(argv[1], "author"))
            {
                char author[0x101];
                nacpGetLanguageEntry(&buf->nacp, &langentry);
                memset(author, 0, sizeof(author));
                strncpy(author, langentry->author, sizeof(author));
                printf("%s\n", author);
            }
            if (!strcmp(argv[1], "name"))
            {
                char name[0x201];
                nacpGetLanguageEntry(&buf->nacp, &langentry);
                memset(name, 0, sizeof(name));
                strncpy(name, langentry->name, sizeof(name));
                printf("%s\n", name);
            }
        }
        free(buf);
    }
    //configure <mainLoopSleepTime or buttonClickSleepTime> <time in ms>
    if(!strcmp(argv[0], "configure")){
        if(argc != 3)
            return 0;

        if(!strcmp(argv[1], "mainLoopSleepTime")){
            u64 time = parseStringToInt(argv[2]);
            mainLoopSleepTime = time;
        }

        if(!strcmp(argv[1], "buttonClickSleepTime")){
            u64 time = parseStringToInt(argv[2]);
            buttonClickSleepTime = time;
        }

        if(!strcmp(argv[1], "echoCommands")){
            u64 shouldActivate = parseStringToInt(argv[2]);
            echoCommands = shouldActivate != 0;
        }

        if(!strcmp(argv[1], "printDebugResultCodes")){
            u64 shouldActivate = parseStringToInt(argv[2]);
            debugResultCodes = shouldActivate != 0;
        }
        
        if(!strcmp(argv[1], "keySleepTime")){
            u64 keyTime = parseStringToInt(argv[2]);
            keyPressSleepTime = keyTime;
        }

        if(!strcmp(argv[1], "fingerDiameter")){
            u32 fDiameter = (u32) parseStringToInt(argv[2]);
            fingerDiameter = fDiameter;
        }

        if(!strcmp(argv[1], "pollRate")){
            u64 fPollRate = parseStringToInt(argv[2]);
            pollRate = fPollRate;
        }

        if(!strcmp(argv[1], "freezeRate")){
            u64 fFreezeRate = parseStringToInt(argv[2]);
            freezeRate = fFreezeRate;
        }

        if(!strcmp(argv[1], "controllerType")){
            detachController();
            u8 fControllerType = (u8)parseStringToInt(argv[2]);
            
            #ifdef __cplusplus
            controllerInitializedType = static_cast<HidDeviceType>(fControllerType);
            #else
            controllerInitializedType = fControllerType;
            #endif
        }

        if(!strcmp(argv[1], "frameAdvanceWaitTimeNs")){
            u32 fFrameAdvanceWaitTimeNs = (u32)parseStringToInt(argv[2]);
            frameAdvanceWaitTimeNs = fFrameAdvanceWaitTimeNs;
        }
    }

    if(!strcmp(argv[0], "getSystemLanguage")){
        //thanks zaksa
        setInitialize();
        u64 languageCode = 0;   
        SetLanguage language = SetLanguage_ENUS;
        setGetSystemLanguage(&languageCode);   
        setMakeLanguage(languageCode, &language);
        printf("%d\n", language);
    }

    if(!strcmp(argv[0], "pixelPeek")){
        //errors with 0x668CE, unless debugunit flag is patched
        u64 bSize = 0x7D000;
        #ifdef __cplusplus
        char* buf = static_cast<char*>(malloc(bSize)); 
        #else
        char* buf = malloc(bSize);
        #endif

        u64 outSize = 0;

        Result rc = capsscCaptureForDebug(buf, bSize, &outSize);

        if (R_FAILED(rc) && debugResultCodes)
            printf("capssc, 1204: %d\n", rc);
        
        u64 i;
        for (i = 0; i < outSize; i++)
        {
            printf("%02X", buf[i]);
        }
        printf("\n");

        free(buf);
    }

    if(!strcmp(argv[0], "getVersion")){
        printf("%s\n", VERSION_S);
    }
	
	// follow pointers and print absolute offset (little endian, flip it yourself if required)
	// pointer <first (main) jump> <additional jumps> !!do not add the last jump in pointerexpr here, add it yourself!!
	if (!strcmp(argv[0], "pointer"))
	{
		if(argc < 2)
            return 0;
		s64 jumps[argc-1];
		for (int i = 1; i < argc; i++)
			jumps[i-1] = parseStringToSignedLong(argv[i]);
		u64 solved = followMainPointer(jumps, argc-1);
		printf("%016lX\n", solved);
	}

    // pointerAll <first (main) jump> <additional jumps> <final jump in pointerexpr> 
    // possibly redundant between the one above, one needs to go eventually. (little endian, flip it yourself if required)
	if (!strcmp(argv[0], "pointerAll"))
	{
		if(argc < 3)
            return 0;
        s64 finalJump = parseStringToSignedLong(argv[argc-1]);
        u64 count = argc - 2;
		s64 jumps[count];
		for (int i = 1; i < argc-1; i++)
			jumps[i-1] = parseStringToSignedLong(argv[i]);
		u64 solved = followMainPointer(jumps, count);
        if (solved != 0)
            solved += finalJump;
		printf("%016lX\n", solved);
	}
	
	// pointerRelative <first (main) jump> <additional jumps> <final jump in pointerexpr> 
	// returns offset relative to heap
	if (!strcmp(argv[0], "pointerRelative"))
	{
		if(argc < 3)
            return 0;
        s64 finalJump = parseStringToSignedLong(argv[argc-1]);
        u64 count = argc - 2;
		s64 jumps[count];
		for (int i = 1; i < argc-1; i++)
			jumps[i-1] = parseStringToSignedLong(argv[i]);
		u64 solved = followMainPointer(jumps, count);
        if (solved != 0)
        {
            solved += finalJump;
            // MetaData meta = getMetaData();
            u64 pid = getPID();
            solved -= getHeapBase(pid);
        }
		printf("%016lX\n", solved);
	}

    // pointerPeek <amount of bytes in hex or dec> <first (main) jump> <additional jumps> <final jump in pointerexpr>
    // warning: no validation
    if (!strcmp(argv[0], "pointerPeek"))
	{
		if(argc < 4)
            return 0;
            
        s64 finalJump = parseStringToSignedLong(argv[argc-1]);
		u64 size = parseStringToInt(argv[1]);
        u64 count = argc - 3;
		s64 jumps[count];
		for (int i = 2; i < argc-1; i++)
			jumps[i-2] = parseStringToSignedLong(argv[i]);
		u64 solved = followMainPointer(jumps, count);
        solved += finalJump;
        peek(solved, size);
	}

    // pointerPeekMulti <amount of bytes in hex or dec> <first (main) jump> <additional jumps> <final jump in pointerexpr> split by asterisks (*)
    // warning: no validation
    if (!strcmp(argv[0], "pointerPeekMulti"))
	{
		if(argc < 4)
            return 0;

        // we guess a max of 40 for now
        u64 offsets[40];
        u64 sizes[40];
        u64 itemCount = 0;

        u64 currIndex = 1;
        u64 lastIndex = 1;

        while (currIndex < argc)
        {
            // count first
            char* thisArg = argv[currIndex];
            while (strcmp(thisArg, "*"))
            {   
                currIndex++;
                if (currIndex < argc)
                    thisArg = argv[currIndex];
                else 
                    break;
            }
            
            u64 thisCount = currIndex - lastIndex;
            
            s64 finalJump = parseStringToSignedLong(argv[currIndex-1]);
            u64 size = parseStringToSignedLong(argv[lastIndex]);
            u64 count = thisCount - 2;
            s64 jumps[count];
            for (int i = 1; i <= count; i++)
                jumps[i-1] = parseStringToSignedLong(argv[i+lastIndex]);
            u64 solved = followMainPointer(jumps, count);
            solved += finalJump;

            offsets[itemCount] = solved;
            sizes[itemCount] = size;
            itemCount++;
            currIndex++;
            lastIndex = currIndex;
        }
        
        peekMulti(offsets, sizes, itemCount);
	}

    // pointerPoke <data to be sent> <first (main) jump> <additional jumps> <final jump in pointerexpr>
    // warning: no validation
    if (!strcmp(argv[0], "pointerPoke"))
	{
		if(argc < 4)
            return 0;
            
        s64 finalJump = parseStringToSignedLong(argv[argc-1]);
        u64 count = argc - 3;
		s64 jumps[count];
		for (int i = 2; i < argc-1; i++)
			jumps[i-2] = parseStringToSignedLong(argv[i]);
		u64 solved = followMainPointer(jumps, count);
        solved += finalJump;

		u64 size;
        u8* data = parseStringToByteBuffer(argv[1], &size);
        poke(solved, size, data);
        free(data);
	}
	
    //touch followed by arrayof: <x in the range 0-1280> <y in the range 0-720>. Array is sequential taps, not different fingers. Functions in its own thread, but will not allow the call again while running. tapcount * pollRate * 2
    if (!strcmp(argv[0], "touch"))
	{
        if(argc < 3 || argc % 2 == 0)
            return 0;

        u32 count = (argc-1)/2;
		
        #ifdef __cplusplus
        HidTouchState* state = static_cast<HidTouchState*>(calloc(count, sizeof(HidTouchState)));
        #else
        HidTouchState* state = calloc(count, sizeof(HidTouchState));
        #endif

        u32 i, j = 0;
        for (i = 0; i < count; ++i)
        {
            state[i].diameter_x = state[i].diameter_y = fingerDiameter;
            state[i].x = (u32) parseStringToInt(argv[++j]);
            state[i].y = (u32) parseStringToInt(argv[++j]);
        }

        makeTouch(state, count, pollRate * 1e+6L, false);
	}

    //touchHold <x in the range 0-1280> <y in the range 0-720> <time in milliseconds (must be at least 15ms)>. Functions in its own thread, but will not allow the call again while running. pollRate + holdtime
    if(!strcmp(argv[0], "touchHold")){
        if(argc != 4)
            return 0;

        #ifdef __cplusplus
        HidTouchState* state = static_cast<HidTouchState*>(calloc(1, sizeof(HidTouchState)));
        #else
        HidTouchState* state = calloc(1, sizeof(HidTouchState));
        #endif

        state->diameter_x = state->diameter_y = fingerDiameter;
        state->x = (u32) parseStringToInt(argv[1]);
        state->y = (u32) parseStringToInt(argv[2]);
        u64 time = parseStringToInt(argv[3]);
        makeTouch(state, 1, time * 1e+6L, false);
    }

    //touchDraw followed by arrayof: <x in the range 0-1280> <y in the range 0-720>. Array is vectors of where finger moves to, then removes the finger. Functions in its own thread, but will not allow the call again while running. (vectorcount * pollRate * 2) + pollRate
    if (!strcmp(argv[0], "touchDraw"))
	{
        if(argc < 3 || argc % 2 == 0)
            return 0;

        u32 count = (argc-1)/2;
		
        #ifdef __cplusplus
        HidTouchState* state = static_cast<HidTouchState*>(calloc(count, sizeof(HidTouchState)));
        #else
        HidTouchState* state = calloc(count, sizeof(HidTouchState));
        #endif

        u32 i, j = 0;
        for (i = 0; i < count; ++i)
        {
            state[i].diameter_x = state[i].diameter_y = fingerDiameter;
            state[i].x = (u32) parseStringToInt(argv[++j]);
            state[i].y = (u32) parseStringToInt(argv[++j]);
        }

        makeTouch(state, count, pollRate * 1e+6L * 2, true);
	}

    if (!strcmp(argv[0], "touchCancel"))
        touchToken = 1;

    //key followed by arrayof: <HidKeyboardKey> to be pressed in sequential order
    //thank you Red (hp3721) for this functionality
    if (!strcmp(argv[0], "key"))
	{
        if (argc < 2)
            return 0;
        
        u64 count = argc-1;

        #ifdef __cplusplus
        HiddbgKeyboardAutoPilotState* keystates = static_cast<HiddbgKeyboardAutoPilotState*>(calloc(count, sizeof (HiddbgKeyboardAutoPilotState)));
        #else
        HiddbgKeyboardAutoPilotState* keystates = calloc(count, sizeof (HiddbgKeyboardAutoPilotState));
        #endif

        u64 i;
        for (i = 0; i < count; i++)
        {
            u8 key = (u8) parseStringToInt(argv[i+1]);
            if (key < 4 || key > 231)
                continue;
            keystates[i].keys[key / 64] = 1UL << key;
            keystates[i].modifiers = 1024UL; //numlock
        }

        makeKeys(keystates, count);
    }

    //keyMod followed by arrayof: <HidKeyboardKey> <HidKeyboardModifier>(without the bitfield shift) to be pressed in sequential order
    if (!strcmp(argv[0], "keyMod"))
	{
        if (argc < 3 || argc % 2 == 0)
            return 0;

        u32 count = (argc-1)/2;
        #ifdef __cplusplus
        HiddbgKeyboardAutoPilotState* keystates = static_cast<HiddbgKeyboardAutoPilotState*>(calloc(count, sizeof (HiddbgKeyboardAutoPilotState)));
        #else
        HiddbgKeyboardAutoPilotState* keystates = calloc(count, sizeof (HiddbgKeyboardAutoPilotState));
        #endif

        u64 i, j = 0;
        for (i = 0; i < count; i++)
        {
            u8 key = (u8) parseStringToInt(argv[++j]);
            if (key < 4 || key > 231)
                continue;
            keystates[i].keys[key / 64] = 1UL << key;
            keystates[i].modifiers = BIT((u8) parseStringToInt(argv[++j]));
        }

        makeKeys(keystates, count);
    }

    //keyMulti followed by arrayof: <HidKeyboardKey> to be pressed at the same time.
    if (!strcmp(argv[0], "keyMulti"))
	{
        if (argc < 2)
            return 0;
        
        u64 count = argc-1;
        
        #ifdef __cplusplus
        HiddbgKeyboardAutoPilotState* keystate = static_cast<HiddbgKeyboardAutoPilotState*>(calloc(1, sizeof (HiddbgKeyboardAutoPilotState)));
        #else
        HiddbgKeyboardAutoPilotState* keystate = calloc(1, sizeof (HiddbgKeyboardAutoPilotState));
        #endif

        u64 i;
        for (i = 0; i < count; i++)
        {
            u8 key = (u8) parseStringToInt(argv[i+1]);
            if (key < 4 || key > 231)
                continue;
            keystate[0].keys[key / 64] |= 1UL << key;
        }

        makeKeys(keystate, 1);
    }
    
    if (!strcmp(argv[0], "charge"))
	{
        u32 charge;
        Result rc = psmInitialize();
        if (R_FAILED(rc))
            fatalThrow(rc);
        psmGetBatteryChargePercentage(&charge);
        printf("%d\n", charge);
        psmExit();
    }

    if (!strcmp(argv[0], "fdCount"))
	{
        printf("%d\n", fd_count);
    }

    if (!strcmp(argv[0], "setControllerState"))
    {
        if (argc != 6) return 0;
        u64 btnState = parseStringToInt(argv[1]);
        int joy_l_x = strtol(argv[2], NULL, 0);
        int joy_l_y = strtol(argv[3], NULL, 0);
        int joy_r_x = strtol(argv[4], NULL, 0);
        int joy_r_y = strtol(argv[5], NULL, 0);
        setControllerState((HidNpadButton)(btnState), joy_l_x, joy_l_y, joy_r_x, joy_r_y);

    }
    if (!strcmp(argv[0], "resetControllerState")) resetControllerState();

    if (!strcmp(argv[0], "loadTAS"))
    {
        if (argc != 2) return 0;
        loadTAS(argv[1]);
    }

    return 0;
}

int main()
{
    #ifdef __cplusplus
    char *linebuf = static_cast<char*>(malloc(sizeof(char) * MAX_LINE_LENGTH));
    pfds = static_cast<pollfd*>(malloc(sizeof *pfds * fd_size));
    #else
    char *linebuf = malloc(sizeof(char) * MAX_LINE_LENGTH);
    pfds = malloc(sizeof *pfds * fd_size);
    #endif

    int c = sizeof(struct sockaddr_in);
    struct sockaddr_in client;

    int listenfd = setupServerSocket(6000);
    pfds[0].fd = listenfd;
    pfds[0].events = POLLIN;
    fd_count = 1;

    int clientfd;
	
	Result rc;
	int fr_count = 0;
	
    initFreezes();

	// freeze thread
	mutexInit(&freezeMutex);
	rc = threadCreate(&freezeThread, sub_freeze, (void*)&freeze_thr_state, NULL, THREAD_SIZE, 0x2C, -2); 
	if (R_SUCCEEDED(rc))
        rc = threadStart(&freezeThread);

    // touch thread
    mutexInit(&touchMutex);
    rc = threadCreate(&touchThread, sub_touch, (void*)&currentTouchEvent, NULL, THREAD_SIZE, 0x2C, -2); 
    if (R_SUCCEEDED(rc))
        rc = threadStart(&touchThread);

    // key thread
    mutexInit(&keyMutex);
    rc = threadCreate(&keyboardThread, sub_key, (void*)&currentKeyEvent, NULL, THREAD_SIZE, 0x2C, -2); 
    if (R_SUCCEEDED(rc))
        rc = threadStart(&keyboardThread);

    // click sequence thread
    mutexInit(&clickMutex);
    rc = threadCreate(&clickThread, sub_click, (void*)currentClick, NULL, THREAD_SIZE, 0x2C, -2); 
    if (R_SUCCEEDED(rc))
        rc = threadStart(&clickThread);

    // TAS thread (culprit)
    mutexInit(&tasMutex);

    sol::state lua;
    luaInit(lua);
    flashLed();

    while (true)
    {
        poll(pfds, fd_count, -1);
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
                                linebuf[readBytesSoFar - 1] = 0;
                                if (pfds[i].fd == clientfd)
                                {
                                    readEnd = true;
                                    printf("%s\n",linebuf);

                                    dup2(pfds[i].fd, STDOUT_FILENO);

                                    // parseArgs(linebuf, &argmain);

                                    try
                                    {
                                        lua.safe_script(linebuf);
                                    }
                                    catch(const sol::error& e)
                                    {
                                        printf("%s\n", e.what());
                                    }
                                    fflush(stdout);
                                }
                                else { }
                            }
                        }
                    }
                }
            }
        }
		fr_count = getFreezeCount();
		if (fr_count == 0)
			freeze_thr_state = Idle;
		mutexUnlock(&freezeMutex);
        svcSleepThread(mainLoopSleepTime * 1e+6L);
    }
	
	if (R_SUCCEEDED(rc))
    {
	    freeze_thr_state = Exit;
		threadWaitForExit(&freezeThread);
        threadClose(&freezeThread);

        currentTouchEvent.state = 3;
        threadWaitForExit(&touchThread);
        threadClose(&touchThread);

        currentKeyEvent.state = 3;
        threadWaitForExit(&keyboardThread);
        threadClose(&keyboardThread);
        
        clickThreadState = 1;
        threadWaitForExit(&clickThread);
        threadClose(&clickThread);

        // tasThreadState = 1;
        // threadWaitForExit(&tasThread);
        // threadClose(&tasThread);
	}
	
	clearFreezes();
    freeFreezes();
	
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
            touch(touchPtr->states, touchPtr->sequentialCount, touchPtr->holdTime, touchPtr->hold, &touchToken);
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

void sub_key(void *arg)
{
    while (1)
    {
        KeyData* keyPtr = (KeyData*)arg;
        if (keyPtr->state == 1)
        {
            mutexLock(&keyMutex); 
            key(keyPtr->states, keyPtr->sequentialCount);
            free(keyPtr->states);
            keyPtr->state = 0;
            mutexUnlock(&keyMutex);
        }

        svcSleepThread(1e+6L);

        if (keyPtr->state == 3)
            break;
    }
}

void sub_click(void *arg)
{
    while (1)
    {
        if (clickThreadState == 1)
            break;

        if (currentClick != NULL)
        {
            mutexLock(&clickMutex);
            clickSequence(currentClick, &clickToken);
            free(currentClick); currentClick = NULL;
            mutexUnlock(&clickMutex);
            printf("done\n");
        }

        clickToken = 0;

        svcSleepThread(1e+6L);
    }
}

void sub_tas(void *arg)
{
    if (tas_script == NULL) return;
    if (!getIsPaused()) attach();

    prev_frame = 0;
    constexpr int NXTAS_MAX_LINE_LEN = 256;
    char line[NXTAS_MAX_LINE_LEN];
    int line_cnt = 0;

    while (fgets(line, NXTAS_MAX_LINE_LEN, tas_script) != NULL)
    {
        int ret = parseArgs(line, &parseNxTasStr);
        if (ret != 0)
        {
            printf("Error parsing line %d\n", line_cnt);
            printf("%s\n", line);
            detach();
            break;
        }
        line_cnt++;

        if (tasThreadState == 1) break;
    }
    fclose(tas_script);
    tas_script = NULL;
    resetControllerState();
}
