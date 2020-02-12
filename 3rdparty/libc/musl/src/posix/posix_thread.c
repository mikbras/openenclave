#include <syscall.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include "posix_thread.h"
#include "posix_io.h"
#include "posix_syscall.h"
#include "posix_spinlock.h"
#include "pthread_impl.h"

#define USE_PTHREAD_SELF

typedef struct _thread_data thread_data_t;

struct _thread_data
{
    void* self;
    void* pthread_self;
};

/* The thread-data structure for the main thread. */
thread_data_t _main_td =
{
    .self = &_main_td,
    .pthread_self = NULL,
};

int posix_set_thread_area(void* p)
{
    static const int SET_FS = 0x1002;

#ifdef USE_PTHREAD_SELF
    return (int)posix_syscall2(SYS_arch_prctl, SET_FS, (long)p);
#else
    assert(_main_td.pthread_self == NULL);
    _main_td.pthread_self = p;
    return (int)posix_syscall2(SYS_arch_prctl, SET_FS, (long)&_main_td);
#endif
}

struct pthread* posix_pthread_self(void)
{
#ifdef USE_PTHREAD_SELF
    pthread_t td;
    __asm__("mov %%fs:0,%0" : "=r" (td));
    return td;
#else
    thread_data_t* td;
    __asm__("mov %%fs:0,%0" : "=r" (td));
    return td->pthread_self;
#endif
}

int posix_clone(int (*func)(void *), void *stack, int flags, void *arg, ...)
{
    va_list ap;
    pid_t* ptid;
    pid_t* ctid;
    void* tls;

    /* Extrat the arguments. */
    va_start(ap, arg);
    ptid = va_arg(ap, pid_t*);
    tls  = va_arg(ap, void*);
    ctid = va_arg(ap, pid_t*);
    va_end(ap);

#ifdef USE_PTHREAD_SELF
    return __clone(func, stack, flags, arg, ptid, tls, ctid);
#else
    thread_data_t* td;

    /* Create thread-data structure and prepend to list. */
    {
        if (!(td = malloc(sizeof(thread_data_t))))
            return -ENOMEM;

        td->self = td;
        td->pthread_self = tls;
    }

    /* Call the assembly function */
    return __clone(func, stack, flags, arg, ptid, td, ctid);
#endif
}
