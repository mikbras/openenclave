#include <unistd.h>
#include <string.h>
#include "libc.h"
#include "pthread_impl.h"
#include "posix_signal.h"
#include "posix_spinlock.h"

#include "posix_warnings.h"

int* __posix_init_host_uaddr;

int __posix_init_tid;

static int* _trace_ptr;
static posix_spinlock_t _lock;

void posix_set_trace(int val)
{
    posix_spin_lock(&_lock);
    *_trace_ptr = val;
    posix_spin_unlock(&_lock);
}

void posix_init(int* host_uaddr, int* trace_ptr, int tid)
{
    size_t aux[64];
    static const char* _environ[] = { NULL };

    memset(aux, 0, sizeof(aux));

    _trace_ptr = trace_ptr;
    __posix_init_host_uaddr = host_uaddr;
    __posix_init_tid = tid;
    __progname = "unknown";
    __sysinfo = 0;
    __environ = (char**)_environ;
    __hwcap = 0;
    __default_stacksize = 4096;

    libc.auxv = aux;
    libc.page_size = PAGESIZE;
    libc.secure = 0;

    __posix_install_exception_handler();

    /* ATTN: this does not return! */
    __init_tls(aux);
}
