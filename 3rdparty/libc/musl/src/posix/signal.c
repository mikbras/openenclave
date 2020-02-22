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
#include "posix_exception.h"

#include "pthread_impl.h"

/* */
#include "posix_warnings.h"

// ATTN: handle SIG_IGN
// ATTN: handle SIG_DFL

static struct posix_sigaction _table[NSIG];
static posix_spinlock_t _lock;

extern void (*oe_continue_execution_hook)(oe_exception_record_t* rec);

static void _call_sigaction(void (*sigaction)(int, siginfo_t*, void*), int sig)
{
    siginfo_t si = { 0 };
    ucontext_t uc = { 0 };

    posix_printf("_call_sigaction()\n");

    /* Invoke the sigacation funtion */
    (*sigaction)(sig, &si, &uc);

    /* Jump to the function address given by the program counter. */
    if (uc.uc_mcontext.MC_PC)
    {
        void (*func)() = (void*)uc.uc_mcontext.MC_PC;
        func();
        /* does not return */
    }

    oe_abort();
}

extern __thread uint64_t __oe_exception_args[6];

static void _continue_execution_hook(oe_exception_record_t* rec)
{
    posix_printf("_continue_execution_hook()\n");

    if (__oe_exception_args[0] == POSIX_EXCEPTION_SIGACTION)
    {
        int sig = (int)__oe_exception_args[1];

        posix_spin_lock(&_lock);
        uint64_t handler = _table[sig].handler;
        posix_spin_unlock(&_lock);

        /* Invoke the signal handler  */
        rec->context->rip = (uint64_t)_call_sigaction;
        rec->context->rdi = handler;
        rec->context->rsi = (uint64_t)sig;
    }
}

static uint64_t _exception_handler(oe_exception_record_t* exception)
{
    (void)exception;

    if (__oe_exception_args[0] == POSIX_EXCEPTION_SIGACTION)
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

    if (signum >= NSIG || !act)
        return -EINVAL;

    posix_spin_lock(&_lock);
    _table[signum] = *act;
    posix_spin_unlock(&_lock);

    if (posix_rt_sigaction_ocall(&r, signum, act, oldact, sigsetsize) != OE_OK)
        return -EINVAL;

    return r;
}
