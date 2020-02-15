#include <errno.h>

#include <limits.h>

#include <openenclave/enclave.h>
#include <openenclave/corelibc/stdlib.h>

#define OE_BUILD_ENCLAVE
#include <openenclave/internal/thread.h>

#include "futex.h"

#include "posix_warnings.h"
#include "posix_cutex.h"
#include "posix_io.h"
#include "posix_spinlock.h"
#include "posix_trace.h"
#include "posix_time.h"
#include "posix_list.h"

#define NUM_CHAINS 1024

typedef struct _cutex cutex_t;

struct _cutex
{
    cutex_t* next;
    size_t refs;
    int* uaddr;
    oe_cond_t cond;
    oe_mutex_t mutex;
};

static cutex_t* _chains[NUM_CHAINS];
static oe_spinlock_t _lock = OE_SPINLOCK_INITIALIZER;

static cutex_t* _get(int* uaddr)
{
    cutex_t* ret = NULL;
    uint64_t index = ((uint64_t)uaddr >> 4) % NUM_CHAINS;
    cutex_t* cutex;

    oe_spin_lock(&_lock);

    for (cutex_t* p = _chains[index]; p; p = p->next)
    {
        if (p->uaddr == uaddr)
        {
            p->refs++;
            ret = p;
            goto done;
        }
    }

    if (!(cutex = oe_calloc(1, sizeof(cutex_t))))
        goto done;

    cutex->refs = 1;
    cutex->uaddr = uaddr;
    cutex->next = _chains[index];
    _chains[index] = cutex;

    ret = cutex;

done:

    oe_spin_unlock(&_lock);

    return ret;
}

#if 0
static int _put(int* uaddr)
{
    int ret = -1;
    uint64_t index = ((uint64_t)uaddr >> 2) % NUM_CHAINS;
    cutex_t* prev = NULL;

    oe_spin_lock(&_lock);

    for (cutex_t* p = _chains[index]; p; p = p->next)
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
    oe_spin_unlock(&_lock);

    return ret;
}
#endif

int posix_cutex_wait(int* uaddr, int op, int val)
{
    int ret = 0;
    cutex_t* cutex = NULL;

    if (!uaddr || (op != FUTEX_WAIT && op != (FUTEX_WAIT|FUTEX_PRIVATE)))
    {
        ret = -EINVAL;
        goto done;
    }

    if (!(cutex = _get(uaddr)))
    {
        ret = -ENOMEM;
        goto done;
    }

    oe_mutex_lock(&cutex->mutex);
    {
        if (*uaddr != val)
        {
            oe_mutex_unlock(&cutex->mutex);
            ret = EAGAIN;
            goto done;
        }

        if (oe_cond_wait(&cutex->cond, &cutex->mutex) != OE_OK)
        {
            ret = ENOSYS;
            goto done;
        }
    }
    oe_mutex_unlock(&cutex->mutex);

done:

    return ret;
}

int posix_cutex_wake(int* uaddr, int op, int val)
{
    int ret = 0;
    cutex_t* cutex = NULL;

    if (!uaddr || (op != FUTEX_WAKE && op != (FUTEX_WAKE|FUTEX_PRIVATE)))
    {
        ret = -EINVAL;
        goto done;
    }

    if (!(cutex = _get(uaddr)))
    {
        ret = -ENOMEM;
        goto done;
    }

    if (val == INT_MAX)
    {
        if (oe_cond_broadcast(&cutex->cond) != OE_OK)
        {
            ret = -ENOSYS;
            goto done;
        }
    }
    else
    {
        for (int i = 0; i < val; i++)
        {
            if (oe_cond_signal(&cutex->cond) != OE_OK)
            {
                ret = -ENOSYS;
                goto done;
            }
        }
    }

done:

    return ret;
}

int posix_cutex_lock(int* uaddr)
{
    cutex_t* cutex = NULL;

    if (!(cutex = _get(uaddr)))
        return -1;

    if (oe_mutex_lock(&cutex->mutex) != OE_OK)
        return -1;

    return 0;
}

int posix_cutex_unlock(int* uaddr)
{
    cutex_t* cutex = NULL;

    if (!(cutex = _get(uaddr)))
        return -1;

    if (oe_mutex_unlock(&cutex->mutex) != OE_OK)
        return -1;

    return 0;
}
