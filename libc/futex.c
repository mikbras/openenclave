// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#include "../3rdparty/musl/musl/src/internal/futex.h"
#include <errno.h>
#include <openenclave/corelibc/stdlib.h>
#include <openenclave/enclave.h>
#include <openenclave/internal/thread.h>
#include <time.h>

typedef struct _futex futex_t;

struct _futex
{
    futex_t* next;
    futex_t* prev;
    int* uaddr;
    int val;
    oe_cond_t cond;
    oe_mutex_t mutex;
};

static futex_t* _head;
static futex_t* _tail;
static oe_spinlock_t _lock = OE_SPINLOCK_INITIALIZER;

static futex_t* _futex_get(int* uaddr, int val)
{
    futex_t* ret = NULL;
    futex_t* futex = NULL;

    oe_spin_lock(&_lock);
    {
        /* Find the futex on the list with the given uaddr */
        for (futex_t* p = _head; p; p = p->next)
        {
            if (p->uaddr == uaddr)
            {
                futex = p;
                break;
            }
        }

        /* If not found */
        if (!futex)
        {
            /* Create the new futex */
            {
                static oe_cond_t _cond = OE_COND_INITIALIZER;
                static oe_mutex_t _mutex = OE_MUTEX_INITIALIZER;

                if (!(futex = oe_calloc(1, sizeof(futex_t))))
                {
                    oe_spin_unlock(&_lock);
                    goto done;
                }

                futex->uaddr = uaddr;
                futex->val = val;
                futex->cond = _cond;
                futex->mutex = _mutex;
            }

            /* Append to the list. */
            if (_head)
            {
                _tail->next = futex;
                futex->prev = _tail;
                _tail = futex;
            }
            else
            {
                _head = futex;
                _tail = futex;
            }
        }
    }
    oe_spin_unlock(&_lock);

    ret = futex;

done:
    return ret;
}

static int _futex_wait(int* uaddr, int val, const struct timespec* timeout)
{
    int ret = 0;
    futex_t* futex;

    if (!(futex = _futex_get(uaddr, val)))
    {
        ret = ENOMEM;
        goto done;
    }

    if (oe_mutex_lock(&futex->mutex) != OE_OK)
    {
        ret = ENOSYS;
        goto done;
    }

    if (*uaddr != val)
    {
        oe_mutex_unlock(&futex->mutex);
        ret = EAGAIN;
        goto done;
    }

    if (oe_cond_wait(&futex->cond, &futex->mutex) != OE_OK)
    {
        oe_mutex_unlock(&futex->mutex);
        ret = ENOSYS;
        goto done;
    }

    if (oe_mutex_unlock(&futex->mutex) != OE_OK)
    {
        ret = ENOSYS;
        goto done;
    }

done:
    return ret;
}

long oe_syscall_futex(long args[6])
{
    int* uaddr = (int*)args[0];
    int futex_op = (int)args[1];

    switch (futex_op)
    {
        case FUTEX_WAIT:
        case FUTEX_WAIT | FUTEX_PRIVATE:
        {
            int val = (int)args[2];
            const struct timespec* timeout = (const struct timespec*)args[3];
            return _futex_wait(uaddr, val, timeout);
        }
        default:
        {
            break;
        }
    }

    return -1;
}
