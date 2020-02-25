// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#define _GNU_SOURCE

#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <execinfo.h>
#include <openenclave/host.h>
#include <openenclave/internal/error.h>
#include <openenclave/internal/tests.h>
#include <openenclave/internal/calls.h>
#include <openenclave/internal/defs.h>
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
#include "../../../../3rdparty/libc/musl/src/posix/posix_signal.h"

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
    printf("CHILD.START=%d\n", posix_gettid_ocall());
    fflush(stdout);
#endif

    _futex = 0;

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

#if 0
    printf("CHILD.EXIT=%d\n", posix_gettid_ocall());
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
    return (int)syscall(SYS_gettid);
}

int posix_getpid_ocall(void)
{
    return (int)syscall(SYS_getpid);
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
#if 1
    printf("%s(tid=%d, sig=%d)\n", __FUNCTION__, tid, sig);
#endif

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

OE_STATIC_ASSERT(sizeof(struct posix_siginfo) == sizeof(siginfo_t));
OE_STATIC_ASSERT(sizeof(struct posix_ucontext) == sizeof(ucontext_t));
OE_STATIC_ASSERT(sizeof(struct posix_siginfo) == sizeof(siginfo_t));
OE_STATIC_ASSERT(sizeof(struct posix_sigset) == sizeof(sigset_t));

#define ENCLU_ERESUME 3
extern uint64_t OE_AEP_ADDRESS;

int posix_print_backtrace(void)
{
    void* buffer[64];

    int n = backtrace(buffer, sizeof(buffer));

    if (n <= 1)
        return -1;

    char** syms = backtrace_symbols(buffer, n);

    if (!syms)
        return -1;

    printf("=== posix_print_backtrace()\n");

    for (int i = 0; i < n; i++)
        printf("%s\n", syms[i]);

    free(syms);

    fflush(stdout);

    return 0;
}

static __thread struct posix_sigaction_args _enclave_sigaction_args;
static __thread bool _have_enclave_sigaction;

static __thread struct posix_sigaction_args _host_sigaction_args;
static __thread bool _have_host_sigaction;

static void _set_enclave_sigaction(int sig, siginfo_t* si, ucontext_t* uc)
{
    _enclave_sigaction_args.sig = sig;

    if (si)
        memcpy(&_enclave_sigaction_args.siginfo, si, sizeof(struct posix_siginfo));
    else
        memset(&_enclave_sigaction_args.siginfo, 0, sizeof(struct posix_siginfo));

    if (uc)
        memcpy(&_enclave_sigaction_args.ucontext, uc, sizeof(struct posix_ucontext));
    else
        memset(&_enclave_sigaction_args.ucontext, 0, sizeof(struct posix_ucontext));

    _have_enclave_sigaction = true;
}

static void _set_host_sigaction(int sig, siginfo_t* si, ucontext_t* uc)
{
    _host_sigaction_args.sig = sig;

    if (si)
        memcpy(&_host_sigaction_args.siginfo, si, sizeof(struct posix_siginfo));
    else
        memset(&_host_sigaction_args.siginfo, 0, sizeof(struct posix_siginfo));

    if (uc)
        memcpy(&_host_sigaction_args.ucontext, uc, sizeof(struct posix_ucontext));
    else
        memset(&_host_sigaction_args.ucontext, 0, sizeof(struct posix_ucontext));

    _have_host_sigaction = true;
}

static void _sigaction_handler(int sig, siginfo_t* si, ucontext_t* uc)
{
#if 1
    printf("host._sigaction_handler: tid=%d sig=%d\n", posix_gettid_ocall(), sig);
#endif

    /* Build the host exception context */
    oe_host_exception_context_t hec = {0};
    hec.rax = (uint64_t)uc->uc_mcontext.gregs[REG_RAX];
    hec.rbx = (uint64_t)uc->uc_mcontext.gregs[REG_RBX];
    hec.rip = (uint64_t)uc->uc_mcontext.gregs[REG_RIP];

    /* If the signal originated within the enclave */
    if (hec.rax == ENCLU_ERESUME && hec.rip == OE_AEP_ADDRESS)
    {
        _set_enclave_sigaction(sig, si, uc);

        uint64_t action = oe_host_handle_exception(&hec, POSIX_SIGACTION);

        if (action == OE_EXCEPTION_CONTINUE_EXECUTION)
        {
            /* Handled */
            return;
        }

        printf("unhanlded signal: %d\n", sig);
        fflush(stdout);
        abort();
    }
    else
    {
        _set_host_sigaction(sig, si, uc);

#if 0
        printf("*** _sigaction_handler: host signal: sig=%d\n", sig);
        posix_print_backtrace();
#endif
        return;
    }
}

int posix_rt_sigaction_ocall(
    int signum,
    const struct posix_sigaction* pact,
    size_t sigsetsize)
{
    struct posix_sigaction act = *pact;
    extern void posix_restore(void);

#if 1
    printf("%s: solicit: signum=%d\n", __FUNCTION__, signum);
    fflush(stdout);
#endif

    errno = 0;

    act.handler = (uint64_t)_sigaction_handler;
    act.restorer = (uint64_t)posix_restore;

    long r = syscall(SYS_rt_sigaction, signum, &act, NULL, sigsetsize);

    if (r != 0)
        return -errno;

    return 0;
}

int posix_get_sigaction_args_ocall(
    struct posix_sigaction_args* args,
    bool enclave_args)
{
    if (enclave_args)
    {
        if (!_have_enclave_sigaction)
            return -1;

        if (args)
            *args = _enclave_sigaction_args;

        memset(&_enclave_sigaction_args, 0, sizeof(_enclave_sigaction_args));
        _have_enclave_sigaction = false;
    }
    else
    {
        if (!_have_host_sigaction)
            return -1;

        if (args)
            *args = _host_sigaction_args;

        memset(&_host_sigaction_args, 0, sizeof(_host_sigaction_args));
        _have_host_sigaction = false;
    }

    return 0;
}

void posix_dump_sigset(const char* msg, const posix_sigset* set)
{
    printf("%s: ", msg);

    for (int i = 0; i < NSIG; i++)
    {
        if (sigismember((sigset_t*)set, i))
            printf("%d ", i);
    }

    printf("\n");
    fflush(stdout);

}

int posix_rt_sigprocmask_ocall(
    int how,
    const struct posix_sigset* set,
    struct posix_sigset* oldset,
    size_t sigsetsize)
{
    errno = 0;

#if 0
    switch (how)
    {
        case SIG_BLOCK:
            posix_dump_sigset("block", set);
            break;
        case SIG_UNBLOCK:
            posix_dump_sigset("unblock", set);
            break;
        case SIG_SETMASK:
            posix_dump_sigset("setmask", set);
            break;
        default:
            assert("panic" == NULL);
            break;
    }
#endif

    long r = syscall(SYS_rt_sigprocmask, how, set, oldset, sigsetsize);
    return r == 0 ? 0 : -errno;
}
