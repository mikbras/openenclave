#include <stdarg.h>
#include <errno.h>
#include <string.h>

#include <openenclave/enclave.h>
#include <openenclave/internal/sgxtypes.h>
#include <openenclave/corelibc/stdlib.h>

#include "pthread_impl.h"

#include "posix_thread.h"
#include "posix_io.h"
#include "posix_syscall.h"
#include "posix_spinlock.h"
#include "posix_mman.h"
#include "posix_trace.h"
#include "posix_futex.h"
#include "posix_futex.h"
#include "posix_warnings.h"
#include "posix_ocalls.h"
#include "posix_signal.h"

#define MAGIC 0x6a25f0aa

/* The thread info structure for the main thread */
static posix_thread_t _main_thread;

static oe_thread_data_t* _get_oetd(void)
{
    oe_thread_data_t* oetd;

    __asm__("mov %%fs:0,%0" : "=r" (oetd));

    return oetd;
}

posix_thread_t* posix_self(void)
{
    oe_thread_data_t* oetd;

    if (!(oetd = _get_oetd()))
        return NULL;

    return (posix_thread_t*)(oetd->__reserved_0);
}

static int _set_thread_info(posix_thread_t* thread)
{
    oe_thread_data_t* oetd;

    if (!(oetd = _get_oetd()))
        return -1;

    oetd->__reserved_0 = (uint64_t)thread;
    return 0;
}

int posix_gettid(void)
{
    int retval;

    if (posix_gettid_ocall(&retval) != OE_OK)
        return -EINVAL;

    return retval;
}

int posix_getpid(void)
{
    int retval;

    if (posix_getpid_ocall(&retval) != OE_OK)
        return -EINVAL;

    return retval;
}

extern int* __posix_init_host_uaddr;

int posix_set_tid_address(int* tidptr)
{
    posix_thread_t* thread;

    if (!(thread = posix_self()))
    {
        oe_assert(false);
        return -EINVAL;
    }

    thread->ctid = tidptr;

    return posix_gettid();
}

/* This is called only by the main thread. */
int posix_set_thread_area(void* p)
{
    memset(&_main_thread, 0, sizeof(_main_thread));
    _main_thread.magic = MAGIC;
    _main_thread.td = (pthread_t)p;
    _main_thread.host_uaddr = __posix_init_host_uaddr;

    _set_thread_info(&_main_thread);

    return 0;
}

struct pthread* posix_pthread_self(void)
{
    posix_thread_t* thread;

    if (!(thread = posix_self()))
        return NULL;

    return thread->td;
}

int posix_run_thread_ecall(uint64_t cookie, int* host_uaddr)
{
    posix_thread_t* thread = (posix_thread_t*)cookie;

    if (!thread || !oe_is_within_enclave(thread, sizeof(thread)) ||
        thread->magic != MAGIC)
    {
        oe_assert(false);
        return -1;
    }

    thread->host_uaddr = host_uaddr;

    _set_thread_info(thread);

    /* Set the TID for this thread */
    a_swap(thread->ptid, posix_gettid());

    if (setjmp(thread->jmpbuf) == 0)
    {
        /* Invoke the MUSL thread wrapper function. */
        (*thread->fn)(thread->arg);

        /* Never returns. */
        posix_printf("unexpected\n");
        oe_abort();
    }

    return 0;
}

int posix_clone(
    int (*fn)(void *),
    void* child_stack,
    int flags,
    void* arg,
    ...)
{
    int ret = 0;
    va_list ap;

    /* Ignored */
    (void)child_stack;

    va_start(ap, arg);
    pid_t* ptid = va_arg(ap, pid_t*);
    struct pthread* td = va_arg(ap, void*);
    pid_t* ctid = va_arg(ap, pid_t*);
    va_end(ap);

    oe_assert(td != NULL);
    oe_assert(td->self == td);

    /* Create the thread info structure for the new thread */
    posix_thread_t* thread;
    {
        if (!(thread = oe_calloc(1, sizeof(posix_thread_t))))
        {
            ret = -ENOMEM;
            goto done;
        }

        thread->magic = MAGIC;
        thread->td = td;
        thread->fn = fn;
        thread->arg = arg;
        thread->flags = flags;
        thread->ptid = ptid;
        thread->ctid = ctid;
    }

    /* Ask the host to call posix_run_thread_ecall() on a new thread */
    {
        int retval = -1;
        uint64_t cookie = (uint64_t)thread;

        if (posix_start_thread_ocall(&retval, cookie) != OE_OK)
        {
            ret = -ENOMEM;
            goto done;
        }

        if (retval != 0)
        {
            ret = -ENOMEM;
            goto done;
        }
    }

done:

    return ret;
}

void posix_exit(int status)
{
    posix_thread_t* thread;

    /* ATTN: ignored */
    (void)status;

    thread = posix_self();
    oe_assert(thread);

    /* ATTN: handle main thread exits */
    if (!thread->fn)
    {
        posix_printf("posix_exit() called from main thread\n");
        oe_abort();
    }

    /* Clear ctid: */
    posix_futex_acquire(thread->ctid);
    a_swap(thread->ctid, 0);
    posix_futex_release(thread->ctid);

    /* Wake the joiner */
    posix_futex_acquire((volatile int*)thread->ctid);
    posix_futex_wake((int*)thread->ctid, FUTEX_WAKE, 1);
    posix_futex_release((volatile int*)thread->ctid);

    /* Hack attempt to release joiner */
#if 0
    struct pthread* td = thread->td;
    ACQUIRE_FUTEX(&td->detach_state);
    int state = a_cas(&td->detach_state, DT_JOINABLE, DT_EXITING);
    (void)state;
    __wake(&td->detach_state, 1, 1);
    RELEASE_FUTEX(&td->detach_state);
#endif

    /* Jump back to posix_run_thread_ecall() */
    longjmp(thread->jmpbuf, 1);
}

long posix_get_robust_list(
    int pid,
    struct posix_robust_list_head** head_ptr,
    size_t* len_ptr)
{
    posix_thread_t* self = posix_self();

    if (pid != 0 || !(self = posix_self()))
        return -EINVAL;

    if (head_ptr)
        *head_ptr = self->robust_list_head;

    if (len_ptr)
        *len_ptr = self->robust_list_len;

    return 0;
}

long posix_set_robust_list(struct posix_robust_list_head* head, size_t len)
{
    posix_printf("head=%p\n", head);
    posix_printf("len=%zu\n", len);

    posix_thread_t* self = posix_self();

    if (!(self = posix_self()))
        return -EINVAL;

    self->robust_list_head = head;
    self->robust_list_len = len;

    return 0;
}

int posix_tkill(int tid, int sig)
{
    int retval;

    if (posix_tkill_ocall(&retval, tid, sig) != OE_OK)
        return -ENOSYS;

    posix_dispatch_signals();
    return retval;
}

void posix_noop(void)
{
    posix_noop_ocall();
}
