// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#include <limits.h>
#include <errno.h>
#include <openenclave/host.h>
#include <openenclave/internal/error.h>
#include <openenclave/internal/tests.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include "posix_u.h"

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

    printf("CHILD.PID=%d\n", posix_gettid_ocall());

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

static void _sigaction_handler(int sig, siginfo_t* si, void* ctx)
{
    (void)sig;
    (void)si;
    (void)ctx;
    printf("_sigaction_handler: tid=%d\n", posix_gettid_ocall());
}

static void _sigaction_restorer()
{
    printf("_sigaction_restorer: tid=%d\n", posix_gettid_ocall());
}

int posix_rt_sigaction_ocall(
    int signum,
    const struct posix_sigaction* pact,
    struct posix_sigaction* poldact,
    size_t sigsetsize)
{
    struct posix_sigaction act = *pact;
    struct posix_sigaction oldact;

    /* ATTN */
    (void)poldact;

    act.handler = (uint64_t)_sigaction_handler;

    /* ATTN: need to use assembly routine */
    act.restorer = (uint64_t)_sigaction_restorer;
    act.restorer = 0;

    long r = syscall(SYS_rt_sigaction, signum, &act, &oldact, sigsetsize);

    if (r != 0)
        return -errno;

    return 0;
}
