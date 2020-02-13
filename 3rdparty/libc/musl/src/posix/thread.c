#include <syscall.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <elf.h>
#include <string.h>
#include <assert.h>
#include <openenclave/internal/sgxtypes.h>
#include "libc.h"
#include "pthread_impl.h"
#include "posix_thread.h"
#include "posix_io.h"
#include "posix_syscall.h"
#include "posix_spinlock.h"
#include "posix_mman.h"
#include "posix_trace.h"

#define MAX_THREAD_INFO 1024

typedef struct _thread_info
{
    pid_t tid;
    int (*func)(void*);
    void* arg;
    int flags;
    pid_t* ptid;
    pid_t* ctid;
    struct pthread* td;
}
thread_info_t;

static thread_info_t _thread_info[MAX_THREAD_INFO];
static posix_spinlock_t _lock;

static thread_info_t* _get_thread_info(void)
{
    thread_info_t* ti = NULL;

    posix_spin_lock(&_lock);
    {
        for (int i = 0; i < MAX_THREAD_INFO; i++)
        {
            if (_thread_info[i].tid == 0)
            {
                ti = &_thread_info[i];
                ti->tid = i + 1;
                break;
            }
        }
    }
    posix_spin_unlock(&_lock);

    return ti;
}

static int _put_thread_info(uint64_t tid)
{
    size_t index;

    if (!(tid > 0 && tid < MAX_THREAD_INFO))
        return -1;

    index = tid - 1;

    posix_spin_lock(&_lock);
    _thread_info[index].tid = 0;
    posix_spin_unlock(&_lock);

    return 0;
}

int posix_set_thread_area(void* p)
{
    pthread_t td = (pthread_t)p;
    oe_thread_data_t* oetd;

    __asm__("mov %%fs:0,%0" : "=r" (oetd));

    oetd->__reserved_0 = (uint64_t)td;

    return 0;
}

oe_thread_data_t* _get_oetd(void)
{
    oe_thread_data_t* oetd;

    __asm__("mov %%fs:0,%0" : "=r" (oetd));

    return oetd;
}

struct pthread* posix_pthread_self(void)
{
    oe_thread_data_t* oetd;
    struct pthread* td;

    __asm__("mov %%fs:0,%0" : "=r" (oetd));

    td = (struct pthread*)oetd->__reserved_0;
    assert(td != NULL);

    return td;
}

int posix_run_thread_ecall(int tid)
{
    thread_info_t* ti;
    oe_thread_data_t* oetd = _get_oetd();
    int r;

    if (!oetd)
        return -1;

    if (tid <= 0 || tid > MAX_THREAD_INFO)
        return -1;

    ti = &_thread_info[tid-1];

    assert(ti->td != NULL);

    oetd->__reserved_0 = (uint64_t)ti->td;

/*
MEB:
*/
    r = (*ti->func)(ti->arg);

    return 0;
}

int posix_clone(int (*func)(void *), void *stack, int flags, void *arg, ...)
{
    int ret = 0;
    va_list ap;
    pid_t* ptid;
    pid_t* ctid;
    struct pthread* td;
    thread_info_t* ti;
    uint64_t tid;
    extern oe_result_t posix_start_thread_ocall(int* retval, int tid);

    va_start(ap, arg);
    ptid = va_arg(ap, pid_t*);
    td = va_arg(ap, void*);
    ctid = va_arg(ap, pid_t*);
    va_end(ap);

    assert(td != NULL);
    assert(td->self == td);

    /* Create the thread info structure */
    {
        if (!(ti = _get_thread_info()))
        {
            ret = -ENOMEM;
            goto done;
        }

        ti->func = func;
        ti->arg = arg;
        ti->flags = flags;
        ti->ptid = ptid;
        ti->ctid = ctid;
        ti->td = td;
    }

    /* Ask the host to call posix_run_thread_ecall() on a new thread. */
    {
        int retval = -1;

        if (posix_start_thread_ocall(&retval, ti->tid) != OE_OK)
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
