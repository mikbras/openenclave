#include <syscall.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "libc.h"
#include "pthread_impl.h"
#include "thread.h"
#include "io.h"
#include "syscall.h"
#include "spinlock.h"
#include "mman.h"

struct posix_pthread
{
    struct pthread base;
    struct pthread* pthread;
};

static struct posix_pthread* _clone_td(struct pthread* td)
{
    size_t tls_size;
    uint8_t* p;
    struct posix_pthread* new;

    if (!td || td->self != td)
        return NULL;

    tls_size = (uint8_t*)td - (uint8_t*)td->dtv;

    if (!(p = malloc(tls_size + sizeof(struct posix_pthread))))
        return NULL;

    memcpy(p, td->dtv, tls_size + sizeof(struct pthread));
    new = (struct posix_pthread*)(p + tls_size);
    new->base.self = &new->base;
    new->pthread = td;

    return new;
}

int posix_set_thread_area(void* p)
{
    static const int SET_FS = 0x1002;
    pthread_t td = (pthread_t)p;
    struct posix_pthread* new;

    if (!(new = _clone_td(td)))
        return -ENOMEM;

    return (int)posix_syscall2(SYS_arch_prctl, SET_FS, (long)new);
}

struct pthread* posix_pthread_self(void)
{
    struct posix_pthread* ptd;
    __asm__("mov %%fs:0,%0" : "=r" (ptd));
    assert(ptd->pthread);

    return ptd->pthread;
}

int posix_clone(int (*func)(void *), void *stack, int flags, void *arg, ...)
{
    va_list ap;
    pid_t* ptid;
    pid_t* ctid;
    struct pthread* td;
    struct posix_pthread* new;

    va_start(ap, arg);
    ptid = va_arg(ap, pid_t*);
    td = va_arg(ap, void*);
    ctid = va_arg(ap, pid_t*);
    va_end(ap);

    assert(td != NULL);
    assert(td->self == td);

    if (!(new = _clone_td(td)))
        return -ENOMEM;

    /* Call the assembly function */
    return __clone(func, stack, flags, arg, ptid, new, ctid);
}
