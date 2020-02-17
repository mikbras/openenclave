#include <unistd.h>
#include <string.h>
#include <assert.h>
#include "libc.h"
#include "pthread_impl.h"
#include "posix_thread.h"
#include "posix_io.h"
#include "posix_trace.h"

#include "posix_warnings.h"

void posix_init(void)
{
    size_t aux[64];
    memset(aux, 0, sizeof(aux));

    __progname = "unknown";
    __sysinfo = 0;
    __environ = NULL;
    __hwcap = 0;
    __default_stacksize = 4096;

    libc.auxv = aux;
    libc.page_size = PAGESIZE;
    libc.secure = 0;

    /* ATTN: this does not return! */
    __init_tls(aux);
}
