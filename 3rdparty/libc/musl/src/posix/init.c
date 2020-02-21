#include <unistd.h>
#include <string.h>
#include <assert.h>
#include "libc.h"
#include "pthread_impl.h"
#include "posix_thread.h"
#include "posix_io.h"
#include "posix_trace.h"
#include <openenclave/enclave.h>
#include <openenclave/internal/calls.h>

#include "posix_warnings.h"

int* __posix_init_host_uaddr;

static uint64_t _exception_handler(oe_exception_record_t* exception)
{
    (void)exception;
    return OE_EXCEPTION_CONTINUE_EXECUTION;
#if 0
    oe_context_t* context = exception->context;

    if (exception->code == OE_EXCEPTION_ILLEGAL_INSTRUCTION)
    {
        return OE_EXCEPTION_CONTINUE_EXECUTION;
    }

    return OE_EXCEPTION_CONTINUE_SEARCH;
#endif
}

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

    if (oe_add_vectored_exception_handler(true, _exception_handler) != OE_OK)
    {
        posix_printf("oe_add_vectored_exception_handler() failed\n");
        oe_abort();
    }

    /* ATTN: this does not return! */
    __init_tls(aux);
}
