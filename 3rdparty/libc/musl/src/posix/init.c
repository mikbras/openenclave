#include <unistd.h>
#include <string.h>
#include <assert.h>
#include "libc.h"
#include "posix_thread.h"
#include "posix_io.h"
#include "posix_trace.h"

void posix_init(volatile int* uaddrs, size_t uaddrs_size)
{
    size_t aux[64];
    memset(aux, 0, sizeof(aux));

    posix_init_uaddrs(uaddrs, uaddrs_size);

    __progname = "unknown";
    __sysinfo = 0;
    __environ = NULL;
    __hwcap = 0;

    libc.auxv = aux;
    libc.page_size = PAGESIZE;
    libc.secure = 0;

    /* ATTN: this does not return! */
    __init_tls(aux);
}
