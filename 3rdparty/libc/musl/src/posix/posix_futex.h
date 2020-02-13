#ifndef _POSIX_FUTEX_H
#define _POSIX_FUTEX_H

#include <time.h>

int posix_futex_wait(
    int* uaddr,
    int futex_op,
    int val,
    const struct timespec* timeout);

#endif /* _POSIX_FUTEX_H */
