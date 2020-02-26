#define _GNU_SOURCE

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <openenclave/enclave.h>
#include <openenclave/internal/calls.h>
#include <openenclave/internal/jump.h>
#include <openenclave/corelibc/stdlib.h>
#include "posix_signal.h"
#include "posix_ocalls.h"
#include "posix_io.h"
#include "posix_thread.h"
#include "posix_spinlock.h"
#include "posix_panic.h"
#include "posix_jump.h"

#include "pthread_impl.h"

/* */
#include "posix_warnings.h"

#define STACK_SIZE (64*1024)

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

void posix_set_trace(int val);

static __thread ucontext_t _ucontext;

static void _enclave_signal_handler(
    void (*sigaction)(int, siginfo_t*, void*),
    int sig,
    siginfo_t* siginfo,
    ucontext_t* ucontext)
{
    posix_printf("_enclave_signal_handler()\n");

    (void)siginfo;
    (void)ucontext;

    posix_set_trace(0x55);

    /* ATTN: use siginfo and ucontext */
    siginfo_t si = { 0 };
    ucontext_t uc = _ucontext;

    /* Invoke the sigacation funtion */
    posix_printf("RIP:1{%llx}\n", uc.uc_mcontext.gregs[REG_RIP]);
    (*sigaction)(sig, &si, &uc);
    posix_printf("RIP:2{%llx}\n", uc.uc_mcontext.gregs[REG_RIP]);

    posix_set_trace(0xa1);

    posix_jump_context_t ctx;
    ctx.rsp = (uint64_t)uc.uc_mcontext.gregs[REG_RSP];
    ctx.rbp = (uint64_t)uc.uc_mcontext.gregs[REG_RBP];
    ctx.rip = (uint64_t)uc.uc_mcontext.gregs[REG_RIP];
    ctx.rbx = (uint64_t)uc.uc_mcontext.gregs[REG_RBX];
    ctx.r12 = (uint64_t)uc.uc_mcontext.gregs[REG_R12];
    ctx.r13 = (uint64_t)uc.uc_mcontext.gregs[REG_R13];
    ctx.r14 = (uint64_t)uc.uc_mcontext.gregs[REG_R14];
    ctx.r15 = (uint64_t)uc.uc_mcontext.gregs[REG_R15];

    if (!oe_is_within_enclave((void*)ctx.rip, 16))
    {
        posix_printf("***RIP is outside the encalve\n");
        oe_abort();
    }

    if (!oe_is_within_enclave((void*)ctx.rsp, 16))
    {
        posix_printf("***RSP is outside the encalve\n");
        oe_abort();
    }

    if (!oe_is_within_enclave((void*)ctx.rbp, 16))
    {
        posix_printf("***RBP is outside the encalve\n");
        oe_abort();
    }

    posix_set_trace(0xa2);
    posix_jump(&ctx);
    posix_set_trace(0xa3);
}

extern uint64_t __oe_exception_arg;

#if 0
static void _continue_execution_hook(oe_exception_record_t* rec)
{
#if 1
    //posix_printf("_continue_execution_hook(tid=%d)\n", posix_gettid());
#endif

    if (__oe_exception_arg == POSIX_SIGACTION)
    {
        posix_printf("_continue_execution_hook:a\n");
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
            rec->context->rip = (uint64_t)_enclave_signal_handler;
            rec->context->rdi = handler;
            rec->context->rsi = (uint64_t)sig;
            rec->context->rdx = (uint64_t)&siginfo;
            rec->context->rcx = (uint64_t)&ucontext;
        }
    }
    else
    {
        posix_printf("_continue_execution_hook:x\n");
    }
}
#endif

static uint64_t _exception_handler(oe_exception_record_t* rec)
{
    (void)rec;

    posix_set_trace(0xbb);

    if (__oe_exception_arg == POSIX_SIGACTION)
    {
        int sig = SIGCANCEL;

        /* Get the handler from the table. */
        posix_spin_lock(&_lock);
        uint64_t handler = _table[sig].handler;
        posix_spin_unlock(&_lock);

posix_set_trace(0xcc);
        /* Invoke the signal handler  */
        if (handler)
        {
            uint8_t* stack;

posix_set_trace(0xdd);
            /* ATTN: release this later */
            if (!(stack = oe_memalign(64, STACK_SIZE)))
            {
posix_set_trace(0xdd1);
                oe_abort();
            }

posix_set_trace(0xdd2);
            memset(stack, 0, STACK_SIZE);
posix_set_trace(0xdd3);

            _ucontext.uc_mcontext.gregs[REG_R8] = (int64_t)rec->context->r8;
            _ucontext.uc_mcontext.gregs[REG_R9] = (int64_t)rec->context->r9;
            _ucontext.uc_mcontext.gregs[REG_R10] = (int64_t)rec->context->r10;
            _ucontext.uc_mcontext.gregs[REG_R11] = (int64_t)rec->context->r11;
            _ucontext.uc_mcontext.gregs[REG_R12] = (int64_t)rec->context->r12;
            _ucontext.uc_mcontext.gregs[REG_R13] = (int64_t)rec->context->r13;
            _ucontext.uc_mcontext.gregs[REG_R14] = (int64_t)rec->context->r14;
            _ucontext.uc_mcontext.gregs[REG_R15] = (int64_t)rec->context->r15;
            _ucontext.uc_mcontext.gregs[REG_RDI] = (int64_t)rec->context->rdi;
            _ucontext.uc_mcontext.gregs[REG_RSI] = (int64_t)rec->context->rsi;
            _ucontext.uc_mcontext.gregs[REG_RBP] = (int64_t)rec->context->rbp;
            _ucontext.uc_mcontext.gregs[REG_RBX] = (int64_t)rec->context->rbx;
            _ucontext.uc_mcontext.gregs[REG_RDX] = (int64_t)rec->context->rdx;
            _ucontext.uc_mcontext.gregs[REG_RAX] = (int64_t)rec->context->rax;
            _ucontext.uc_mcontext.gregs[REG_RCX] = (int64_t)rec->context->rcx;
            _ucontext.uc_mcontext.gregs[REG_RSP] = (int64_t)rec->context->rsp;
            _ucontext.uc_mcontext.gregs[REG_RIP] = (int64_t)rec->context->rip;

posix_set_trace(0xdd4);

            rec->context->rsp = (uint64_t)stack + (STACK_SIZE / 2);
            rec->context->rbp = (uint64_t)stack + (STACK_SIZE / 2);
            rec->context->rip = (uint64_t)_enclave_signal_handler;
            rec->context->rdi = handler;
            rec->context->rsi = (uint64_t)sig;
            rec->context->rdx = 0;
            rec->context->rcx = 0;
posix_set_trace(0xee);
        }

        return OE_EXCEPTION_CONTINUE_EXECUTION;
    }
    else
    {
        return OE_EXCEPTION_CONTINUE_SEARCH;
    }
}

void __posix_install_exception_handler(void)
{
#if 0
    oe_continue_execution_hook = _continue_execution_hook;
#endif

    if (oe_add_vectored_exception_handler(false, _exception_handler) != OE_OK)
    {
        posix_printf("oe_add_vectored_exception_handler() failed\n");
        oe_abort();
    }

    posix_set_trace(0xaa);
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

#if 0
int posix_dispatch_signals(void)
{
    posix_printf("*** posix_dispatch_signals()\n");
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
        _enclave_signal_handler(
            (void (*)(int, siginfo_t*, void*))handler,
            args.sig,
            (siginfo_t*)&args.siginfo,
            (ucontext_t*)&args.ucontext);
    }

    return 0;
}
#endif
