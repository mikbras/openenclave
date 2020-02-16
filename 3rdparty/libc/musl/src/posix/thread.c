#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <setjmp.h>

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
#include "posix_cutex.h"
#include "posix_warnings.h"

#define MAGIC 0x6a25f0aa

oe_result_t posix_start_thread_ocall(int* retval, uint64_t cookie);

oe_result_t posix_gettid_ocall(int* retval);

typedef struct _thread_info
{
    /* Should contain MAGIC */
    uint32_t magic;

    /* Pointer to MUSL pthread structure */
    struct pthread* td;

    /* The fn parameter from posix_clone() */
    int (*fn)(void*);

    /* The arg parameter from posix_clone() */
    void* arg;

    /* The flags parameter from posix_clone() */
    int flags;

    /* The ptid parameter from posix_clone() */
    pid_t* ptid;

    /* The ctid parameter from posix_clone() (__UADDR(__thread_list_lock) */
    volatile pid_t* ctid;

    /* Used to jump from posix_exit() back to posix_run_thread_ecall() */
    jmp_buf jmpbuf;
}
thread_info_t;

/* The thread info structure for the main thread */
static thread_info_t _main_thread_info;

static oe_thread_data_t* _get_oetd(void)
{
    oe_thread_data_t* oetd;

    __asm__("mov %%fs:0,%0" : "=r" (oetd));

    return oetd;
}

static thread_info_t* _get_thread_info(void)
{
    oe_thread_data_t* oetd;

    if (!(oetd = _get_oetd()))
        return NULL;

    return (thread_info_t*)(oetd->__reserved_0);
}

static int _set_thread_info(thread_info_t* ti)
{
    oe_thread_data_t* oetd;

    if (!(oetd = _get_oetd()))
        return -1;

    oetd->__reserved_0 = (uint64_t)ti;
    return 0;
}

int posix_gettid(void)
{
    int retval;

    if (posix_gettid_ocall(&retval) != OE_OK)
        return -EINVAL;

    return retval;
}

int posix_set_tid_address(int* tidptr)
{
    thread_info_t* ti;

    if (!(ti = _get_thread_info()))
    {
        oe_assert(false);
        return -EINVAL;
    }

    ti->ctid = tidptr;

    return posix_gettid();
}

/* This is called only by the main thread. */
int posix_set_thread_area(void* p)
{
    memset(&_main_thread_info, 0, sizeof(_main_thread_info));
    _main_thread_info.magic = MAGIC;
    _main_thread_info.td = (pthread_t)p;
    _set_thread_info(&_main_thread_info);

    return 0;
}

struct pthread* posix_pthread_self(void)
{
    thread_info_t* ti;

    if (!(ti = _get_thread_info()))
        return NULL;

    return ti->td;
}

int posix_run_thread_ecall(uint64_t cookie)
{
    thread_info_t* ti = (thread_info_t*)cookie;

    if (!ti || !oe_is_within_enclave(ti, sizeof(ti)) || ti->magic != MAGIC)
    {
        oe_assert(false);
        return -1;
    }

    _set_thread_info(ti);

    /* Set the TID for this thread */
    a_swap(ti->ptid, posix_gettid());

    if (setjmp(ti->jmpbuf) == 0)
    {
        /* Invoke the MUSL thread wrapper function. */
        (*ti->fn)(ti->arg);

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
    thread_info_t* ti;
    {
        if (!(ti = oe_calloc(1, sizeof(thread_info_t))))
        {
            ret = -ENOMEM;
            goto done;
        }

        ti->magic = MAGIC;
        ti->td = td;
        ti->fn = fn;
        ti->arg = arg;
        ti->flags = flags;
        ti->ptid = ptid;
        ti->ctid = ctid;
    }

    /* Ask the host to call posix_run_thread_ecall() on a new thread */
    {
        int retval = -1;
        uint64_t cookie = (uint64_t)ti;

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
    thread_info_t* ti;

    /* ATTN: ignored */
    (void)status;

    ti = _get_thread_info();
    oe_assert(ti);

    /* ATTN: handle main thread exits */
    if (!ti->fn)
    {
        posix_printf("posix_exit() called from main thread\n");
        oe_abort();
    }

    /* Clear ctid: */
    posix_cutex_lock(ti->ctid);
    a_swap(ti->ctid, 0);
    posix_cutex_unlock(ti->ctid);

    /* Wake the joiner */
    posix_cutex_wake((int*)ti->ctid, FUTEX_WAKE, 1);

    /* Jump back to posix_run_thread_ecall() */
    longjmp(ti->jmpbuf, 1);
}
