#ifndef _POSIX_CUTEX_H
#define _POSIX_CUTEX_H

#include <time.h>

int posix_cutex_wait(int* uaddr, int futex_op, int val);

int posix_cutex_wake(int* uaddr, int futex_op, int val);

int posix_cutex_lock(volatile int* uaddr);

int posix_cutex_unlock(volatile int* uaddr);

#endif /* _POSIX_CUTEX_H */
