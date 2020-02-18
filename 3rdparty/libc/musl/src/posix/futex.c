#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <openenclave/enclave.h>
#include <openenclave/corelibc/stdlib.h>
#include <openenclave/internal/thread.h>
#include <openenclave/internal/sgxtypes.h>
#include "futex.h"
#include "posix_warnings.h"
#include "posix_futex.h"
#include "posix_io.h"
#include "posix_spinlock.h"
#include "posix_trace.h"
#include "posix_time.h"
#include "posix_mutex.h"
#include "posix_cond.h"

#define NUM_CHAINS 1024

typedef struct _futex futex_t;

struct _futex
{
    futex_t* next;
    size_t refs;
    int* uaddr;
    posix_cond_t cond;
    posix_mutex_t mutex;
};

static futex_t* _chains[NUM_CHAINS];
static posix_spinlock_t _lock = POSIX_SPINLOCK_INITIALIZER;

static futex_t* _get(int* uaddr)
{
    futex_t* ret = NULL;
    uint64_t index = ((uint64_t)uaddr >> 4) % NUM_CHAINS;
    futex_t* futex;

    posix_spin_lock(&_lock);

    for (futex_t* p = _chains[index]; p; p = p->next)
    {
        if (p->uaddr == uaddr)
        {
            p->refs++;
            ret = p;
            goto done;
        }
    }

    if (!(futex = oe_calloc(1, sizeof(futex_t))))
        goto done;

    futex->refs = 1;
    futex->uaddr = uaddr;
    futex->next = _chains[index];
    _chains[index] = futex;

    ret = futex;

done:

    posix_spin_unlock(&_lock);

    return ret;
}

#if 0
static int _put(int* uaddr)
{
    int ret = -1;
    uint64_t index = ((uint64_t)uaddr >> 2) % NUM_CHAINS;
    futex_t* prev = NULL;

    posix_spin_lock(&_lock);

    for (futex_t* p = _chains[index]; p; p = p->next)
    {
        if (p->uaddr == uaddr)
        {
            p->refs--;

            if (p->refs == 0)
            {
                if (prev)
                    prev->next = p->next;
                else
                    _chains[index] = p->next;

                oe_free(p);
            }

            ret = 0;
            goto done;
        }

        prev = p;
    }

done:
    posix_spin_unlock(&_lock);

    return ret;
}
#endif

static bool _is_ownwer(posix_mutex_t* m)
{
    return posix_self() == m->owner;
}

int posix_futex_wait(
    int* uaddr,
    int op,
    int val,
    const struct timespec *timeout)
{
    int ret = 0;
    futex_t* futex = NULL;

    if (timeout)
    {
        posix_printf("posix_futex_wait(): timeout not supported yet\n");
        posix_print_backtrace();
        oe_abort();
    }

    if (!uaddr || (op != FUTEX_WAIT && op != (FUTEX_WAIT|FUTEX_PRIVATE)))
    {
        ret = -EINVAL;
        goto done;
    }

    if (!(futex = _get(uaddr)))
    {
        ret = -ENOMEM;
        goto done;
    }

    posix_mutex_lock(&futex->mutex);
    {
        if (*uaddr != val)
        {
            posix_mutex_unlock(&futex->mutex);
            ret = EAGAIN;
            goto done;
        }

        if (posix_cond_wait(&futex->cond, &futex->mutex) != 0)
        {
            ret = ENOSYS;
            goto done;
        }
    }

    posix_mutex_unlock(&futex->mutex);

done:

    return ret;
}

int posix_futex_wake(int* uaddr, int op, int val)
{
    int ret = 0;
    futex_t* futex = NULL;


    if (!uaddr || (op != FUTEX_WAKE && op != (FUTEX_WAKE|FUTEX_PRIVATE)))
    {
        ret = -EINVAL;
        goto done;
    }

    if (!(futex = _get(uaddr)))
    {
        ret = -ENOMEM;
        goto done;
    }

    if (!_is_ownwer(&futex->mutex))
    {
        posix_printf("posix_futex_wake(): caller does not own this mutex\n");
        posix_print_backtrace();
        oe_abort();
    }

    if (val == INT_MAX)
    {
        if (posix_cond_broadcast(&futex->cond) != 0)
        {
            ret = -ENOSYS;
            goto done;
        }
    }
    else
    {
        for (int i = 0; i < val; i++)
        {
            if (posix_cond_signal(&futex->cond) != 0)
            {
                ret = -ENOSYS;
                goto done;
            }
        }
    }

done:

    return ret;
}

int posix_futex_acquire(volatile int* uaddr)
{
    futex_t* futex = NULL;

    if (!(futex = _get((int*)uaddr)))
        return -1;

    if (posix_mutex_lock(&futex->mutex) != 0)
        return -1;

    return 0;
}

int posix_futex_release(volatile int* uaddr)
{
    futex_t* futex = NULL;

    if (!(futex = _get((int*)uaddr)))
        return -1;

    if (posix_mutex_unlock(&futex->mutex) != 0)
        return -1;

    return 0;
}
