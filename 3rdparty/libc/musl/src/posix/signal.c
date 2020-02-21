#define _GNU_SOURCE

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <openenclave/enclave.h>
#include "posix_signal.h"
#include "posix_ocalls.h"
#include "posix_io.h"
#include "posix_thread.h"
#include "pthread_impl.h"
#include <openenclave/enclave.h>
#include <openenclave/internal/calls.h>
#include <openenclave/corelibc/stdlib.h>

/* */
#include "posix_warnings.h"

static int _signum;
static struct posix_sigaction _act;

extern void (*oe_continue_execution_hook)(oe_exception_record_t* rec);

static void _call_sigaction(
    void (*sigaction)(int sig, siginfo_t* si, void* ctx),
    int sig,
    siginfo_t* si,
    void* ctx)
{
    ucontext_t* uc = ctx;

    posix_printf("_call_sigaction()\n");

    (void)sigaction;
    (void)sig;
    (void)si;
    (void)ctx;

    sigaction(sig, si, ctx);

    void (*func)() = (void (*)())uc->uc_mcontext.MC_PC;

printf("<<<<<<<<<<\n");
    func();
printf(">>>>>>>>>>\n");
}

static void _continue_execution_hook(oe_exception_record_t* rec)
{
    /* Invoke the original signal handler. */

    posix_printf("_continue_execution_hook()\n");

    static ucontext_t _uc = { 0 };

    rec->context->rip = (uint64_t)_call_sigaction;
    rec->context->rdi = _act.handler;
    rec->context->rsi = 33;
    rec->context->rdx = 0; /* ATTN: */
    rec->context->rcx = (uint64_t)&_uc;
}

static uint64_t _exception_handler(oe_exception_record_t* exception)
{
    (void)exception;
    return OE_EXCEPTION_CONTINUE_EXECUTION;
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

    posix_printf("%s()\n", __FUNCTION__);

    _signum = signum;
    _act = *act;

    if (posix_rt_sigaction_ocall(&r, signum, act, oldact, sigsetsize) != OE_OK)
        return -EINVAL;

    return r;
}
