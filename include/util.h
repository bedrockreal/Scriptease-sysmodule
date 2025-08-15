#include <switch.h>
#include <poll.h>

#define MAX_LINE_LENGTH 344 * 32 * 2

#define R_ASSERT(res_expr)              \
    ({                                  \
        const Result rc = (res_expr);   \
        if (R_FAILED(rc))               \
        {                               \
            fatalThrow(rc);             \
        }                               \
    })

#define R_DEBUG(res_expr)               \
    ({                                  \
        const Result rc = (res_expr);   \
        if (R_FAILED(rc))               \
        {                               \
            printf("%s: 0x%x (%d, %d) (in %s)\n", #res_expr, rc, R_MODULE(rc), R_DESCRIPTION(rc), __PRETTY_FUNCTION__);  \
        }                               \
    })

int setupServerSocket(int port);
HidNpadButton parseStringToButton(char* arg);
Result capsscCaptureForDebug(void *buffer, size_t buffer_size, u64 *size); //big thanks to Behemoth from the Reswitched Discord!
void flashLed(void);

void add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size);
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count);

void reverseArray(u8* arr, int start, int end);

bool getIsProgramOpen(u64 tid);
