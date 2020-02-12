#ifndef _POSIX_THREAD_H
#define _POSIX_THREAD_H

int posix_set_thread_area(void* p);

int posix_clone(int (*func)(void *), void *stack, int flags, void *arg, ...);

#endif /* _POSIX_THREAD_H */
