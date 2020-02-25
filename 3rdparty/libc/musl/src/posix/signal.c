#define _GNU_SOURCE

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <openenclave/enclave.h>
#include <openenclave/enclave.h>
#include <openenclave/internal/calls.h>
#include <openenclave/corelibc/stdlib.h>
#include "posix_signal.h"
#include "posix_ocalls.h"
#include "posix_io.h"
#include "posix_thread.h"
#include "posix_spinlock.h"
#include "posix_panic.h"

#include "pthread_impl.h"

/* */
#include "posix_warnings.h"

// ATTN: handle SIG_IGN
// ATTN: handle SIG_DFL

struct posix_sigaction
{
    uint64_t handler;
    unsigned long flags;
    uint64_t restorer;
    unsigned mask[2];
};

struct posix_sigset
{
    unsigned long __bits[128/sizeof(long)];
};

OE_STATIC_ASSERT(sizeof(sigset_t) == sizeof(posix_sigset_t));

struct posix_siginfo
{
    uint8_t data[128];
};

OE_STATIC_ASSERT(sizeof(struct posix_siginfo) == sizeof(siginfo_t));

struct posix_ucontext
{
    uint8_t data[936];
};

OE_STATIC_ASSERT(sizeof(struct posix_ucontext) == sizeof(ucontext_t));

struct posix_sigaction_args
{
    struct posix_siginfo siginfo;
    struct posix_ucontext ucontext;
    int sig;
};

static struct posix_sigaction _table[NSIG];
static posix_spinlock_t _lock;

extern void (*oe_continue_execution_hook)(oe_exception_record_t* rec);

static void _call_sigaction_handler(
    void (*sigaction)(int, siginfo_t*, void*),
    int sig,
    siginfo_t* siginfo,
    ucontext_t* ucontext)
{
    (void)siginfo;
    (void)ucontext;

    /* ATTN: use siginfo and ucontext */
    siginfo_t si = { 0 };
    ucontext_t uc = { 0 };

#if 1
    posix_printf("enclave._call_sigaction_handler()\n");
#endif

    /* Invoke the sigacation funtion */
    (*sigaction)(sig, &si, &uc);

    /* Jump to the function address given by the program counter. */
    if (uc.uc_mcontext.MC_PC)
    {
        void (*func)() = (void*)uc.uc_mcontext.MC_PC;
        func();

        /* does not return */
        oe_abort();
    }
}

extern __thread uint64_t __oe_exception_arg;

static void _continue_execution_hook(oe_exception_record_t* rec)
{
#if 0
    posix_printf("_continue_execution_hook()\n");
#endif

    if (__oe_exception_arg == POSIX_SIGACTION)
    {
        int sig = 0;
        struct posix_siginfo siginfo = { 0 };
        struct posix_ucontext ucontext = { 0 };

        /* Fetch the sigset handler arguments from the host. */
        {
            oe_result_t result;
            int retval = -1;
            struct posix_sigaction_args args;

            memset(&args, 0xAA, sizeof(args));

            result = posix_get_sigaction_args_ocall(&retval, &args, true);
            if (result != OE_OK || retval != 0)
            {
                POSIX_PANIC;
            }

            sig = args.sig;
            siginfo = args.siginfo;
            ucontext = args.ucontext;
        }

        /* Get the handler from the table. */
        posix_spin_lock(&_lock);
        uint64_t handler = _table[sig].handler;
        posix_spin_unlock(&_lock);

        /* Invoke the signal handler  */
        if (handler)
        {
            rec->context->rip = (uint64_t)_call_sigaction_handler;
            rec->context->rdi = handler;
            rec->context->rsi = (uint64_t)sig;
            rec->context->rdx = (uint64_t)&siginfo;
            rec->context->rcx = (uint64_t)&ucontext;
        }
    }
}

static uint64_t _exception_handler(oe_exception_record_t* rec)
{
    (void)rec;

    if (__oe_exception_arg == POSIX_SIGACTION)
    {
        return OE_EXCEPTION_CONTINUE_EXECUTION;
    }
    else
    {
        return OE_EXCEPTION_CONTINUE_SEARCH;
    }
}

void __posix_install_exception_handler(void)
{
    oe_continue_execution_hook = _continue_execution_hook;

    if (oe_add_vectored_exception_handler(false, _exception_handler) != OE_OK)
    {
        posix_printf("oe_add_vectored_exception_handler() failed\n");
        oe_abort();
    }
}

int posix_rt_sigaction(
    int signum,
    const struct posix_sigaction* act,
    struct posix_sigaction* oldact,
    size_t sigsetsize)
{
    int r;

    if (act)
    {
        if (act->handler == (uint64_t)0)
            POSIX_PANIC;

        if (act->handler == (uint64_t)1)
            POSIX_PANIC;
    }

    if (signum >= NSIG || !act)
        return -EINVAL;

    posix_spin_lock(&_lock);
    {
        if (oldact)
            *oldact = _table[signum];

        if (act)
            _table[signum] = *act;
    }
    posix_spin_unlock(&_lock);

    if (!act)
        return 0;

    if (posix_rt_sigaction_ocall(&r, signum, act, sigsetsize) != OE_OK)
        return -EINVAL;

    return r;
}

int posix_rt_sigprocmask(
    int how,
    const sigset_t* set,
    sigset_t* oldset,
    size_t sigsetsize)
{
    int retval;

    if (posix_rt_sigprocmask_ocall(
        &retval,
        how,
        (const struct posix_sigset*)set,
        (struct posix_sigset*)oldset,
        sigsetsize) != OE_OK)
    {
        return -EINVAL;
    }

    return retval;
}

int posix_dispatch_signals(void)
{
    int retval;
    struct posix_sigaction_args args;

    /* Get the last signal on this thread from the host. */
    if (posix_get_sigaction_args_ocall(&retval, &args, false) != OE_OK ||
        retval != 0)
    {
        return -1;
    }

    /* Get the handler from the table if any */
    posix_spin_lock(&_lock);
    uint64_t handler = _table[args.sig].handler;
    posix_spin_unlock(&_lock);

    /* Invoke the signal handler  */
    if (handler)
    {
        _call_sigaction_handler(
            (void (*)(int, siginfo_t*, void*))handler,
            args.sig,
            (siginfo_t*)&args.siginfo,
            (ucontext_t*)&args.ucontext);
    }

    return 0;
}
