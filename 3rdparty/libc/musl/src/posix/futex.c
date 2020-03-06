#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <assert.h>
#include <openenclave/enclave.h>
#include <openenclave/corelibc/stdlib.h>
#include <openenclave/internal/thread.h>
#include <openenclave/internal/sgxtypes.h>
#include "futex.h"
#include "posix_common.h"
#include "posix_warnings.h"
#include "posix_futex.h"
#include "posix_io.h"
#include "posix_spinlock.h"
#include "posix_time.h"
#include "posix_trace.h"
#include "posix_signal.h"
#include "posix_ocall_structs.h"
#include "posix_thread.h"
#include "posix_panic.h"
#include "posix_ocall_structs.h"
#include "posix_structs.h"

typedef struct _futex futex_t;

struct _futex
{
    futex_t* next;
    volatile int* lock;
    volatile int* uaddr;
};

#define NUM_CHAINS 1024

static futex_t* _chains[NUM_CHAINS];
static posix_spinlock_t _lock = POSIX_SPINLOCK_INITIALIZER;

static size_t _uaddrs_index;

static volatile int* _assign_uaddr(volatile int* lock)
{
    volatile int* ret = NULL;
    posix_thread_t* self;

    if (!(self = posix_self()))
        POSIX_PANIC("unexpected");

    if (!self->shared_block)
        POSIX_PANIC("unexpected");

    if (_uaddrs_index >= self->shared_block->num_uaddrs)
        goto done;

    ret = (volatile int*)&self->shared_block->uaddrs[_uaddrs_index];
    *ret = *lock;
    _uaddrs_index++;

done:
    return ret;
}

volatile int* posix_futex_map(volatile int* lock)
{
    volatile int* ret = NULL;
    uint64_t index = ((uint64_t)lock >> 4) % NUM_CHAINS;
    futex_t* futex;
#if 0
    sigset_t set;
#endif

    if (!oe_is_within_enclave((void*)lock, sizeof(*lock)))
        POSIX_PANIC("lock not within enclave");

#if 0
    __block_all_sigs(&set);
#endif
    posix_lock_signal(0xff);
    posix_spin_lock(&_lock);

    for (futex_t* p = _chains[index]; p; p = p->next)
    {
        if (p->lock == lock)
        {
            ret = p->uaddr;
            goto done;
        }
    }

    if (!(futex = oe_calloc(1, sizeof(futex_t))))
        goto done;

    futex->lock = lock;

    if (!(futex->uaddr = _assign_uaddr(lock)))
        POSIX_PANIC("out of uaddrs");

    futex->next = _chains[index];
    _chains[index] = futex;

    ret = futex->uaddr;

done:

    posix_spin_unlock(&_lock);
    posix_unlock_signal(0xff);
#if 0
    __restore_sigs(&set);
#endif

    return ret;
}

int posix_futex_wait(
    volatile int* uaddr,
    int op,
    int val,
    const struct timespec *timeout)
{
    int ret = 0;
    int r;

    if (!oe_is_outside_enclave((void*)uaddr, sizeof(*uaddr)))
    {
        posix_printf("wait: illeagl uaddr\n");
        posix_print_backtrace();
        POSIX_PANIC("wait: uaddr not outside enclave");
    }

    if (!uaddr || (op != FUTEX_WAIT && op != (FUTEX_WAIT|FUTEX_PRIVATE)))
    {
        ret = -EINVAL;
        goto done;
    }

    if (POSIX_OCALL(posix_wait_ocall(
        &r, (int*)uaddr, val, (posix_timespec_t*)timeout), 0xa80662c7) != OE_OK)
    {
        ret = -EINVAL;
        goto done;
    }

    if (r != 0)
    {
        ret = r;
        goto done;
    }

done:

    return ret;
}

int posix_futex_wake(volatile int* uaddr, int op, int val)
{
    int ret = 0;
    int r;

    if (!oe_is_outside_enclave((void*)uaddr, sizeof(*uaddr)))
    {
        posix_printf("wait: illeagl uaddr\n");
        posix_print_backtrace();
        POSIX_PANIC("wake: uaddr not outside enclave");
    }

    if (!uaddr || (op != FUTEX_WAKE && op != (FUTEX_WAKE|FUTEX_PRIVATE)))
    {
        ret = -EINVAL;
        goto done;
    }
    if (POSIX_OCALL(posix_wake_ocall(
        &r, (int*)uaddr, val), 0xd22ad62b) != OE_OK)
    {
        ret = -EINVAL;
        goto done;
    }

    if (r != 0)
    {
        ret = r;
        goto done;
    }

done:

    return ret;
}

int posix_futex_requeue(
    int* uaddr,
    int op,
    int val,
    int val2,
    int* uaddr2)
{
    int ret = 0;
    int r;

    if (!uaddr || !uaddr2)
    {
        ret = -EINVAL;
        goto done;
    }

    if (!oe_is_outside_enclave((void*)uaddr, sizeof(*uaddr)))
        POSIX_PANIC("wake: uaddr not outside enclave");

    if (!oe_is_outside_enclave((void*)uaddr2, sizeof(*uaddr2)))
        POSIX_PANIC("wake: uaddr2 not outside enclave");

    if (op != FUTEX_REQUEUE && op != (FUTEX_REQUEUE|FUTEX_PRIVATE))
    {
        ret = -EINVAL;
        goto done;
    }

    if ((val < 0 && val != INT_MAX) || (val2 < 0 && val != INT_MAX))
    {
        ret = -EINVAL;
        goto done;
    }

    if (POSIX_OCALL(posix_futex_requeue_ocall(
        &r, (int*)uaddr, val, val2, uaddr2), 0xa836050e) != OE_OK)
    {
        ret = -EINVAL;
        goto done;
    }

    if (r != 0)
    {
        ret = r;
        goto done;
    }

done:
    return ret;
}
