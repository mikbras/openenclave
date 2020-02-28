#define _GNU_SOURCE

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <openenclave/enclave.h>
#include <openenclave/internal/calls.h>
#include <openenclave/internal/jump.h>
#include <openenclave/corelibc/stdlib.h>
#include <openenclave/internal/jump.h>
#include "posix_signal.h"
#include "posix_ocalls.h"
#include "posix_ocall_structs.h"
#include "posix_io.h"
#include "posix_thread.h"
#include "posix_spinlock.h"
#include "posix_panic.h"
#include "posix_jump.h"
#include "posix_ocall_structs.h"

#include "pthread_impl.h"

/* */
#include "posix_warnings.h"

#define STACK_SIZE (64*1024)

// ATTN: handle SIG_IGN
// ATTN: handle SIG_DFL

static bool _inside_enclave_signal_handler;

OE_STATIC_ASSERT(sizeof(sigset_t) == sizeof(posix_sigset_t));
OE_STATIC_ASSERT(sizeof(struct posix_siginfo) == sizeof(siginfo_t));
OE_STATIC_ASSERT(sizeof(struct posix_ucontext) == sizeof(ucontext_t));

static struct posix_sigaction _table[NSIG];
static posix_spinlock_t _lock;

extern void (*oe_continue_execution_hook)(oe_exception_record_t* rec);

typedef void (*sigaction_handler_t)(int, siginfo_t*, void*);

static ucontext_t _ucontext;

int posix_fetch_and_clear_sig_args(struct posix_sig_args* args)
{
    posix_thread_t* self = posix_self();

    if (!args || !self || !self->shared_block)
        return -1;

    *args = self->shared_block->sig_args;
    memset(&self->shared_block->sig_args, 0, sizeof(struct posix_sig_args));

    return 0;
}

static void _enclave_signal_handler(
    void (*sigaction)(int, siginfo_t*, void*),
    int sig,
    siginfo_t* siginfo,
    ucontext_t* ucontext)
{
    struct posix_sig_args args;

    posix_printf("_enclave_signal_handler()\n");

    (void)siginfo;
    (void)ucontext;

    if (posix_fetch_and_clear_sig_args(&args) != 0)
    {
        POSIX_PANIC("unexpected");
    }

    /* ATTN: use siginfo and ucontext */
    siginfo_t si = { 0 };
    //ucontext_t uc = _ucontext;
    ucontext_t uc = { 0 };

    /* Invoke the sigacation funtion */
    posix_printf("BEFORE.SIGHANDLER{%llx}\n", uc.uc_mcontext.gregs[REG_RIP]);
    _inside_enclave_signal_handler = true;
    (*sigaction)(sig, &si, &uc);
    _inside_enclave_signal_handler = false;
    posix_printf("AFTER.SIGHANDLER{%llx}\n", uc.uc_mcontext.gregs[REG_RIP]);


#if 1
    {
        posix_jump_context_t ctx;

        if (uc.uc_mcontext.gregs[REG_RIP])
            ctx.rip = (uint64_t)uc.uc_mcontext.gregs[REG_RIP];
        else
            ctx.rip = (uint64_t)_ucontext.uc_mcontext.gregs[REG_RIP];

        ctx.rsp = (uint64_t)_ucontext.uc_mcontext.gregs[REG_RSP];
        ctx.rbp = (uint64_t)_ucontext.uc_mcontext.gregs[REG_RBP];
        ctx.rbx = (uint64_t)_ucontext.uc_mcontext.gregs[REG_RBX];
        ctx.r12 = (uint64_t)_ucontext.uc_mcontext.gregs[REG_R12];
        ctx.r13 = (uint64_t)_ucontext.uc_mcontext.gregs[REG_R13];
        ctx.r14 = (uint64_t)_ucontext.uc_mcontext.gregs[REG_R14];
        ctx.r15 = (uint64_t)_ucontext.uc_mcontext.gregs[REG_R15];

        if (!oe_is_within_enclave((void*)ctx.rip, 16))
        {
            posix_printf("***RIP is outside the enclave\n");
            oe_abort();
        }

        if (!oe_is_within_enclave((void*)ctx.rsp, 16))
        {
            posix_printf("***RSP is outside the enclave\n");
            oe_abort();
        }

        if (!oe_is_within_enclave((void*)ctx.rbp, 16))
        {
            posix_printf("***RBP is outside the enclave\n");
            oe_abort();
        }

        posix_jump(&ctx);
    }
#endif


    if (uc.uc_mcontext.gregs[REG_RIP])
    {

        void (*func)() = (void (*)())uc.uc_mcontext.gregs[REG_RIP];
        func();
    }
    else
    {
        posix_force_exit(0);
    }
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
            struct posix_sig_args args;

            memset(&args, 0xAA, sizeof(args));

            result = posix_fetch_and_clear_sig_args_ocall(&retval, &args, true);
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

    if (__oe_exception_arg == POSIX_SIGACTION)
    {
        int sig = SIGCANCEL;

        /* Get the handler from the table. */
        posix_spin_lock(&_lock);
        uint64_t handler = _table[sig].handler;
        posix_spin_unlock(&_lock);

        /* Invoke the signal handler  */
        if (handler)
        {
            uint8_t* stack;

            /* ATTN: release this later */
            if (!(stack = oe_memalign(64, STACK_SIZE)))
            {
                oe_abort();
            }

            memset(stack, 0, STACK_SIZE);

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

            rec->context->rsp = (uint64_t)stack + (STACK_SIZE / 2);
            rec->context->rbp = (uint64_t)stack + (STACK_SIZE / 2);
            rec->context->rip = (uint64_t)_enclave_signal_handler;
            rec->context->rdi = handler;
            rec->context->rsi = (uint64_t)sig;
            rec->context->rdx = 0;
            rec->context->rcx = 0;
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
            POSIX_PANIC("unimplemented");

        if (act->handler == (uint64_t)1)
            POSIX_PANIC("unimplemented");
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

int posix_dispatch_signal(void)
{
    sigaction_handler_t handler;
    oe_jmpbuf_t env;
    struct posix_sig_args args;

    posix_printf("posix_dispatch_signal()\n");

    if (posix_fetch_and_clear_sig_args(&args) != 0)
        POSIX_PANIC("unexpected");

    if (args.sig == 0)
        return 0;

    if (args.enclave_sig)
        return 0;

    posix_printf("DISPATCH\n");

#if 0
    if (_inside_enclave_signal_handler)
        return 0;
#endif

    /* Get the signal handler from the table */
    {
        posix_spin_lock(&_lock);
        handler = (sigaction_handler_t)_table[args.sig].handler;
        posix_spin_unlock(&_lock);

        if (!handler)
            POSIX_PANIC("handler not found");
    }

    /* Build a ucontext and invoke the signal handler */
    if (oe_setjmp(&env) == 0)
    {
        siginfo_t si;
        ucontext_t uc;

        memcpy(&si, &args.siginfo, sizeof(si));

        memset(&uc, 0, sizeof(uc));
        uc.uc_mcontext.gregs[REG_RSP] = (int64_t)env.rsp;
        uc.uc_mcontext.gregs[REG_RBP] = (int64_t)env.rbp;
        uc.uc_mcontext.gregs[REG_RIP] = (int64_t)env.rip;
        uc.uc_mcontext.gregs[REG_RBX] = (int64_t)env.rbx;
        uc.uc_mcontext.gregs[REG_R12] = (int64_t)env.r12;
        uc.uc_mcontext.gregs[REG_R13] = (int64_t)env.r13;
        uc.uc_mcontext.gregs[REG_R14] = (int64_t)env.r14;
        uc.uc_mcontext.gregs[REG_R15] = (int64_t)env.r15;

        /* Invoke the signal handler */
        handler(args.sig, &si, &uc);

        env.rsp = (uint64_t)uc.uc_mcontext.gregs[REG_RSP];
        env.rbp = (uint64_t)uc.uc_mcontext.gregs[REG_RBP];
        env.rip = (uint64_t)uc.uc_mcontext.gregs[REG_RIP];
        env.rbx = (uint64_t)uc.uc_mcontext.gregs[REG_RBX];
        env.r12 = (uint64_t)uc.uc_mcontext.gregs[REG_R12];
        env.r13 = (uint64_t)uc.uc_mcontext.gregs[REG_R13];
        env.r14 = (uint64_t)uc.uc_mcontext.gregs[REG_R14];
        env.r15 = (uint64_t)uc.uc_mcontext.gregs[REG_R15];

        oe_longjmp(&env, 1);
        POSIX_PANIC("unreachable");
    }

    /* control continues here if hanlder didn't change RIP */

    return 0;
}
