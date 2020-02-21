#include <unistd.h>
#include <string.h>
#include "libc.h"
#include "pthread_impl.h"
#include "posix_signal.h"

#include "posix_warnings.h"

int* __posix_init_host_uaddr;

void posix_init(int* host_uaddr)
{
    size_t aux[64];

    memset(aux, 0, sizeof(aux));

    __posix_init_host_uaddr = host_uaddr;
    __progname = "unknown";
    __sysinfo = 0;
    __environ = NULL;
    __hwcap = 0;
    __default_stacksize = 4096;

    libc.auxv = aux;
    libc.page_size = PAGESIZE;
    libc.secure = 0;

    __posix_install_exception_handler();

    /* ATTN: this does not return! */
    __init_tls(aux);
}
