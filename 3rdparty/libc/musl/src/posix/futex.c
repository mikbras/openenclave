#include <openenclave/enclave.h>
#include <errno.h>
#include <assert.h>
#include "posix_futex.h"
#include "posix_io.h"
#include "posix_spinlock.h"
#include "futex.h"
#include "posix_trace.h"
#include "posix_time.h"

oe_result_t posix_futex_wait_ocall(
    int* retval,
    int* uaddr,
    int futex_op,
    int val,
    const struct posix_timespec* timeout);

static volatile int* _uaddrs;
static size_t _uaddrs_size;
static int _uaddrs_next = 1;
static posix_spinlock_t _lock;

void posix_init_uaddrs(volatile int* uaddrs, size_t uaddrs_size)
{
    _uaddrs = uaddrs;
    _uaddrs_size = uaddrs_size;
    _uaddrs_next = 1;

    assert(_uaddrs != NULL);
    assert(_uaddrs_size > 0);
    assert(_uaddrs_next > 0);
    assert(_uaddrs_next != _uaddrs_size);
}

volatile int* posix_futex_uaddr(volatile int* uaddr)
{
    volatile int* ptr = NULL;

    posix_spin_lock(&_lock);
    {
        assert(_uaddrs != NULL);
        assert(_uaddrs_size > 0);
        assert(_uaddrs_next > 0);
        assert(_uaddrs_next != _uaddrs_size);

        if (*uaddr == 0)
        {
            if (_uaddrs_next != _uaddrs_size)
            {
                *uaddr = _uaddrs_next++;
                ptr = &_uaddrs[*uaddr];
            }
        }
        else
        {
            if (*uaddr > 0 && *uaddr < _uaddrs_next)
            {
                ptr = &_uaddrs[*uaddr];
            }
        }
    }
    posix_spin_unlock(&_lock);

    assert(ptr);

    return ptr;
}

int posix_futex_wait(
    int* uaddr,
    int futex_op,
    int val,
    const struct timespec* timeout)
{
    if (futex_op == FUTEX_WAIT || futex_op == FUTEX_WAIT|FUTEX_PRIVATE)
    {
        struct posix_timespec posix_timeout_buffer;
        struct posix_timespec* posix_timeout = NULL;
        int retval;

        if (timeout)
        {
            posix_timeout = &posix_timeout_buffer;
            posix_timeout->tv_sec = timeout->tv_sec;
            posix_timeout->tv_nsec = timeout->tv_nsec;
        }

        if (posix_futex_wait_ocall(
            &retval,
            uaddr,
            futex_op,
            val,
            posix_timeout) != OE_OK)
        {
            return -EINVAL;
        }

        return retval;
    }

    return -EINVAL;
}
