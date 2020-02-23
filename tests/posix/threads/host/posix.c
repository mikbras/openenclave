// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#define _GNU_SOURCE

#include <limits.h>
#include <errno.h>
#include <openenclave/host.h>
#include <openenclave/internal/error.h>
#include <openenclave/internal/tests.h>
#include <openenclave/internal/calls.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <ucontext.h>
#include "posix_u.h"
#include "../../../../3rdparty/libc/musl/src/posix/posix_exception.h"

static oe_enclave_t* _enclave = NULL;

extern int posix_init(oe_enclave_t* enclave)
{
    if (!enclave)
        return -1;

    _enclave = enclave;
    return 0;
}

static void* _thread_func(void* arg)
{
    int retval;
    uint64_t cookie = (uint64_t)arg;
    static __thread int _futex;

#if 0
    printf("CHILD.PID=%d\n", posix_gettid_ocall());
    fflush(stdout);
#endif

    if (posix_run_thread_ecall(_enclave, &retval, cookie, &_futex) != OE_OK)
    {
        fprintf(stderr, "posix_run_thread_ecall(): failed\n");
        abort();
    }

    if (retval != 0)
    {
        fprintf(stderr, "posix_run_thread_ecall(): retval=%d\n", retval);
        abort();
    }

#if 1
    printf("child exited=%d\n", posix_gettid_ocall());
    fflush(stdout);
#endif

    return NULL;
}

int posix_start_thread_ocall(uint64_t cookie)
{
    pthread_t t;

    if (pthread_create(&t, NULL, _thread_func, (void*)cookie) != 0)
        return -1;

    return 0;
}

int posix_gettid_ocall(void)
{
    return (int)syscall(__NR_gettid);
}

int posix_nanosleep_ocall(
    const struct posix_timespec* req,
    struct posix_timespec* rem)
{
    if (nanosleep((struct timespec*)req, (struct timespec*)rem) != 0)
        return -errno;

    return 0;
}

int posix_futex_wait_ocall(
    int* uaddr,
    int futex_op,
    int val,
    const struct posix_timespec* timeout)
{
    long r;

    r = syscall(SYS_futex, uaddr, futex_op, val, (struct timespec*)timeout);

    if (r != 0)
        return -errno;

    return 0;
}

int posix_futex_wake_ocall(
    int* uaddr,
    int futex_op,
    int val)
{
    long r;

    r = syscall(SYS_futex, uaddr, futex_op, val, NULL);

    if (r != 0)
        return -errno;

    return 0;
}

int posix_wait_ocall(int* host_uaddr, const struct posix_timespec* timeout)
{
    int retval = 0;

    if (__sync_fetch_and_add(host_uaddr, -1) == 0)
    {
            retval = (int)syscall(
                SYS_futex,
                host_uaddr,
                FUTEX_WAIT_PRIVATE,
                -1,
                timeout,
                NULL,
                0);

        if (retval != 0)
            retval = -errno;
    }

    return retval;
}

void posix_wake_ocall(int* host_uaddr)
{
    if (__sync_fetch_and_add(host_uaddr, 1) != 0)
    {
        syscall(SYS_futex, host_uaddr, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
    }
}

int posix_wake_wait_ocall(
    int* waiter_host_uaddr,
    int* self_host_uaddr,
    const struct posix_timespec* timeout)
{
    posix_wake_ocall(waiter_host_uaddr);
    return posix_wait_ocall(self_host_uaddr, timeout);
}

int posix_clock_gettime_ocall(int clk_id, struct posix_timespec* tp)
{
    return clock_gettime(clk_id, (struct timespec*)tp);
}

int posix_tkill_ocall(int tid, int sig)
{
    printf("%s(tid=%d, sig=%d)\n", __FUNCTION__, tid, sig);

    long r = syscall(SYS_tkill, tid, sig);

    if (r != 0)
        return -errno;

    return 0;
}

/* ATTN: duplicate of OE definition */
typedef struct _host_exception_context
{
    /* exit code */
    uint64_t rax;

    /* tcs address */
    uint64_t rbx;

    /* exit address */
    uint64_t rip;
} oe_host_exception_context_t;

/* ATTN: duplicate of OE definition */
uint64_t oe_host_handle_exception(
    oe_host_exception_context_t* context,
    uint64_t arg);

static void _sigaction_handler(int sig, siginfo_t* si, ucontext_t* ctx)
{
    (void)si;

    printf("_sigaction_handler: tid=%d sig=%d\n", posix_gettid_ocall(), sig);

    oe_host_exception_context_t hec = {0};
    hec.rax = (uint64_t)ctx->uc_mcontext.gregs[REG_RAX];
    hec.rbx = (uint64_t)ctx->uc_mcontext.gregs[REG_RBX];
    hec.rip = (uint64_t)ctx->uc_mcontext.gregs[REG_RIP];

    uint64_t action = oe_host_handle_exception(&hec, (uint64_t)sig);

    if (action == OE_EXCEPTION_CONTINUE_EXECUTION)
        return;

    /* ATTN: handle other non-enclave exceptions */
    printf("exception not handled\n");
    fflush(stdout);
    abort();
}

int posix_rt_sigaction_ocall(
    int signum,
    const struct posix_sigaction* pact,
    struct posix_sigaction* poldact,
    size_t sigsetsize)
{
    struct posix_sigaction act = *pact;
    struct posix_sigaction oldact;
    extern void posix_restore(void);

    /* ATTN */
    (void)poldact;

    act.handler = (uint64_t)_sigaction_handler;
    act.restorer = (uint64_t)posix_restore;

    long r = syscall(SYS_rt_sigaction, signum, &act, &oldact, sigsetsize);

    if (r != 0)
        return -errno;

    return 0;
}
