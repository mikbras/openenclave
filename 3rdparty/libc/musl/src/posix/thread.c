#include <syscall.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <elf.h>
#include <string.h>
#include <assert.h>
#include <setjmp.h>
#include <openenclave/internal/sgxtypes.h>
#include <openenclave/enclave.h>
#include "libc.h"
#include "pthread_impl.h"
#include "posix_thread.h"
#include "posix_io.h"
#include "posix_syscall.h"
#include "posix_spinlock.h"
#include "posix_mman.h"
#include "posix_trace.h"
#include "posix_futex.h"

#define MAX_THREAD_INFO 1024

#define MAGIC 0x6a25f0aa

oe_result_t posix_start_thread_ocall(int* retval, uint64_t cookie);

oe_result_t posix_gettid_ocall(int* retval);

typedef struct _thread_info
{
    uint32_t magic;
    struct pthread* td;

    int (*func)(void*);
    void* arg;
    int flags;
    pid_t* ptid;

    /* Refers to __UADDR(__thread_list_lock) */
    pid_t* ctid;

    jmp_buf jmpbuf;
}
thread_info_t;

static thread_info_t _main_thread_info;

static oe_thread_data_t* _get_oetd(void)
{
    oe_thread_data_t* oetd;

    __asm__("mov %%fs:0,%0" : "=r" (oetd));

    return oetd;
}

static thread_info_t* _get_thread_info(oe_thread_data_t* oetd)
{
    if (!oetd)
        return NULL;

    return (thread_info_t*)(oetd->__reserved_0);
}

static void _set_thread_info(oe_thread_data_t* oetd, thread_info_t* ti)
{
    oetd->__reserved_0 = (uint64_t)ti;
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
    thread_info_t* ti = _get_thread_info(_get_oetd());
    int retval;

    if (!ti)
        return -EINVAL;

    ti->ctid = tidptr;

    if (posix_gettid_ocall(&retval) != OE_OK)
        return -EINVAL;

    return retval;
}

/* This is called only by the main thread. */
int posix_set_thread_area(void* p)
{
    oe_thread_data_t* oetd = _get_oetd();

    if (!oetd)
        return -EINVAL;

    memset(&_main_thread_info, 0, sizeof(_main_thread_info));
    _main_thread_info.magic = MAGIC;
    _main_thread_info.td = (pthread_t)p;
    _set_thread_info(oetd, &_main_thread_info);

    return 0;
}

struct pthread* posix_pthread_self(void)
{
    oe_thread_data_t* oetd = _get_oetd();
    thread_info_t* ti;

    if (!oetd || !(ti = _get_thread_info(oetd)))
        return NULL;

    return ti->td;
}

int posix_run_thread_ecall(uint64_t cookie)
{
    thread_info_t* ti = (thread_info_t*)cookie;
    oe_thread_data_t* oetd = _get_oetd();
    int r;

    if (!ti || !oe_is_within_enclave(ti, sizeof(ti)) || ti->magic != MAGIC)
        return -1;

    if (!oetd)
        return -1;

    _set_thread_info(oetd, ti);

    /* Set the TID */
    a_swap(ti->ptid, posix_gettid());

    if (setjmp(ti->jmpbuf) == 0)
    {
        /* Invoke the MUSL thread wrapper function. */
        r = (*ti->func)(ti->arg);
    }

    posix_printf("posix_run_thread_ecall(): r=%d\n", r);

    return 0;
}

void dump_tl(void)
{
    posix_printf("dump_tl(): __thread_list_lock=%d\n", __thread_list_lock);
}

int posix_clone(int (*func)(void *), void *stack, int flags, void *arg, ...)
{
    int ret = 0;
    va_list ap;


    va_start(ap, arg);
    pid_t* ptid = va_arg(ap, pid_t*);
    struct pthread* td = va_arg(ap, void*);
    pid_t* ctid = va_arg(ap, pid_t*);
    va_end(ap);

    assert(td != NULL);
    assert(td->self == td);

    /* Create the thread info structure for the new thread */
    thread_info_t* ti;
    {
        if (!(ti = calloc(1, sizeof(thread_info_t))))
        {
            ret = -ENOMEM;
            goto done;
        }

        ti->magic = MAGIC;
        ti->td = td;
        ti->func = func;
        ti->arg = arg;
        ti->flags = flags;
        ti->ptid = ptid;
        ti->ctid = ctid;
    }

    /* Ask the host to call posix_run_thread_ecall() on a new thread. */
    {
        int retval = -1;

        if (posix_start_thread_ocall(&retval, (uint64_t)ti) != OE_OK)
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

#if 0
    posix_printf("%s(): ret=%d tid=%d\n", __FUNCTION__, ret, ti->tid);
#endif

    return ret;
}

void posix_exit(int status)
{
    oe_thread_data_t* oetd = _get_oetd();
    thread_info_t* ti;

    assert(oetd);

    ti = _get_thread_info(_get_oetd());

    /* If this is not the main thread. */
    if (ti->func)
    {
#if 0
        posix_printf("EXITING CHILD: %d\n", *ti->ctid);
#endif

        /* Clear ctid: */
        a_swap(ti->ctid, 0);

        /* Wak the joiner. */
        posix_futex_wake(ti->ctid, FUTEX_WAKE, 1);

        /* Jump back to the thread routine */
        longjmp(ti->jmpbuf, 1);
    }
    else
    {
#if 0
        posix_printf("EXITING PARENT: %d\n", *ti->ctid);
#endif
    }
}
