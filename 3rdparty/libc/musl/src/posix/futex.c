#include <errno.h>

#include <openenclave/enclave.h>
#include <openenclave/corelibc/stdlib.h>

#include "futex.h"

#include "posix_warnings.h"
#include "posix_futex.h"
#include "posix_io.h"
#include "posix_spinlock.h"
#include "posix_trace.h"
#include "posix_time.h"

#define UADDR_OFFSET 1024

oe_result_t posix_futex_wait_ocall(
    int* retval,
    int* uaddr,
    int futex_op,
    int val,
    const struct posix_timespec* timeout);

oe_result_t posix_futex_wake_ocall(
    int* retval,
    int* uaddr,
    int futex_op,
    int val);

static volatile int* _host_uaddrs;
static size_t _uaddrs_size;
static size_t _uaddrs_next_index;
static posix_spinlock_t _lock;

OE_INLINE bool _is_host_uaddr(volatile int* uaddr)
{
    return uaddr >= _host_uaddrs && uaddr < &_host_uaddrs[_uaddrs_size];
}

void posix_init_uaddrs(volatile int* uaddrs, size_t uaddrs_size)
{
    oe_assert(uaddrs != NULL);
    oe_assert(uaddrs_size > 0);

    _host_uaddrs = uaddrs;
    _uaddrs_size = uaddrs_size;
}

volatile int* posix_futex_uaddr(volatile int* uaddr)
{
    volatile int* ptr = NULL;

    /* Map the enclave address to a host address */
    posix_spin_lock(&_lock);
    {
        if (*uaddr < UADDR_OFFSET)
        {
            if (_uaddrs_next_index != _uaddrs_size)
            {
                _host_uaddrs[_uaddrs_next_index] = *uaddr;
                *uaddr = (int)_uaddrs_next_index + UADDR_OFFSET;
                ptr = &_host_uaddrs[_uaddrs_next_index];
                _uaddrs_next_index++;
            }
        }
        else if (*uaddr < (int)_uaddrs_size + UADDR_OFFSET)
        {
            ptr = &_host_uaddrs[*uaddr - UADDR_OFFSET];
        }
    }
    posix_spin_unlock(&_lock);

    if (!ptr)
    {
        posix_printf("posix_futex_uaddr() panic");
        posix_print_backtrace();
        oe_abort();
    }

    return ptr;
}

int posix_futex_wait(
    int* uaddr,
    int futex_op,
    int val,
    const struct timespec* timeout)
{
    if (futex_op == FUTEX_WAIT || futex_op == (FUTEX_WAIT|FUTEX_PRIVATE))
    {
        int retval;

        if (!_is_host_uaddr(uaddr))
        {
            posix_printf("posix_futex_wait(): bad uaddr\n");
            posix_print_backtrace();
            oe_abort();
        }

        if (posix_futex_wait_ocall(
            &retval,
            uaddr,
            futex_op,
            val,
            (const struct posix_timespec*)timeout) != OE_OK)
        {
            oe_assert("unexpected" == NULL);
            return -EINVAL;
        }

        return retval;
    }

    return -EINVAL;
}

int posix_futex_wake(
    int* uaddr,
    int futex_op,
    int val)
{
    if (futex_op == FUTEX_WAKE || futex_op == (FUTEX_WAKE|FUTEX_PRIVATE))
    {
        int retval;

        if (!_is_host_uaddr(uaddr))
        {
            posix_printf("posix_futex_wake(): bad uaddr\n");
            posix_print_backtrace();
            oe_abort();
        }

        if (posix_futex_wake_ocall(&retval, uaddr, futex_op, val) != OE_OK)
        {
            oe_assert("unexpected" == NULL);
            return -EINVAL;
        }

        return retval;
    }

    return -EINVAL;
}
