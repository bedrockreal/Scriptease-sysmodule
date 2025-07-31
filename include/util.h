#include <switch.h>
#include <poll.h>

#define MAX_LINE_LENGTH 344 * 32 * 2

#define R_ASSERT(res_expr)            \
    ({                                \
        const Result rc = (res_expr); \
        if (R_FAILED(rc))             \
        {                             \
            fatalThrow(rc);           \
        }                             \
    })

int setupServerSocket(int port);
HidNpadButton parseStringToButton(char* arg);
Result capsscCaptureForDebug(void *buffer, size_t buffer_size, u64 *size); //big thanks to Behemoth from the Reswitched Discord!
void flashLed(void);

void add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size);
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count);

void makeTouch(HidTouchState* state, u64 sequentialCount, u64 holdTime, bool hold);
void reverseArray(u8* arr, int start, int end);
