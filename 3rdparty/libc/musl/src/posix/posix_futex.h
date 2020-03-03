#ifndef _POSIX_FUTEX_H
#define _POSIX_FUTEX_H

#include <time.h>
#include "posix_thread.h"

int posix_futex_wait(
    volatile int* uaddr,
    int futex_op,
    int val,
    const struct timespec *timeout);

int posix_futex_wake(volatile int* uaddr, int futex_op, int val);

int posix_futex_requeue(
    int* uaddr,
    int futex_op,
    int val,
    int val2,
    int* uaddr2);

#endif /* _POSIX_FUTEX_H */
