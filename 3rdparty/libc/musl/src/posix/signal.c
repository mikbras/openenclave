#define _GNU_SOURCE

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <openenclave/enclave.h>
#include <openenclave/internal/calls.h>
#include <openenclave/internal/jump.h>
#include <openenclave/corelibc/stdlib.h>
#include <openenclave/internal/jump.h>
#include "posix_common.h"
#include "posix_signal.h"
#include "posix_ocalls.h"
#include "posix_ocall_structs.h"
#include "posix_io.h"
#include "posix_thread.h"
#include "posix_spinlock.h"
#include "posix_panic.h"
#include "posix_jump.h"
#include "posix_ocall_structs.h"
#include "posix_trace.h"
#include "posix_panic.h"
#include "posix_structs.h"
#include "posix_assert.h"

#include "pthread_impl.h"

/* */
#include "posix_warnings.h"

/* ATTN: handle SIG_IGN */
/* ATTN: handle SIG_DFL */

OE_STATIC_ASSERT(sizeof(sigset_t) == sizeof(posix_sigset_t));
OE_STATIC_ASSERT(sizeof(struct posix_siginfo) == sizeof(siginfo_t));
OE_STATIC_ASSERT(sizeof(struct posix_ucontext) == sizeof(ucontext_t));

static struct posix_sigaction _table[NSIG];
static posix_spinlock_t _lock;

typedef void (*sigaction_handler_t)(int, siginfo_t*, void*);

static __thread ucontext_t _tls_ucontext;

static int _sig_queue_pop_front(posix_sig_queue_node_t* node_out)
{
    int ret = -1;
    posix_shared_block_t* shared_block;
    posix_sig_queue_node_t* node;
    bool locked = false;

    if (!node_out || !(shared_block = posix_shared_block()))
        goto done;

    posix_spin_lock(&shared_block->sig_queue_lock);
    locked = true;

    /* Remove the next node from the queue */
    if (!(node = (posix_sig_queue_node_t*)posix_list_pop_front(
        (posix_list_t*)&shared_block->sig_queue)))
    {
        goto done;
    }

    /* Add node to the free list */
    posix_list_push_back(
        (posix_list_t*)&shared_block->sig_queue_free_list,
        (posix_list_node_t*)node);

    *node_out = *node;

    ret = 0;

done:

    if (locked)
        posix_spin_unlock(&shared_block->sig_queue_lock);

    return ret;
}

static bool _sig_queue_empty(void)
{
    bool ret;
    posix_shared_block_t* shared_block = posix_shared_block();

    posix_spin_lock(&shared_block->sig_queue_lock);
    ret = shared_block->sig_queue.head ? false : true;
    posix_spin_unlock(&shared_block->sig_queue_lock);

    return ret;
}

#if 0
static posix_sig_queue_node_t* _sig_queue_peek_front(void)
{
    posix_sig_queue_node_t* ret = NULL;
    posix_shared_block_t* shared_block;
    bool locked = false;

    if (!(shared_block = posix_shared_block()))
        goto done;

    posix_spin_lock(&shared_block->sig_queue_lock);
    locked = true;

    ret = shared_block->sig_queue.head;

done:

    if (locked)
        posix_spin_unlock(&shared_block->sig_queue_lock);

    return ret;
}
#endif

/*
**==============================================================================
**
** _user_zone_signal_handler()
**
**     This function handles enclave signals that ocurred above the redzone
**     (i.e., above the syscall layer). It invokes the user's signal handler.
**
**==============================================================================
*/

static void _user_zone_signal_handler(void)
{
    posix_sig_queue_node_t node;

    POSIX_ASSERT(posix_shared_block()->zone == POSIX_ZONE_USER);

    if (_sig_queue_pop_front(&node) != 0)
        POSIX_PANIC("unexpected");

#if 0
    posix_printf("_user_zone_signal_handler(): sig=%d\n", node.signum);
#endif

    if (node.signum == 0)
        POSIX_PANIC("unexpected");

    posix_spin_lock(&_lock);
    uint64_t handler = _table[node.signum].handler;
    posix_spin_unlock(&_lock);

    if (!handler)
        POSIX_PANIC("unexpected");

    sigaction_handler_t sigaction = (sigaction_handler_t)handler;
    siginfo_t* si = (siginfo_t*)&node.siginfo;
    ucontext_t* uc = &_tls_ucontext;

    /* Invoke the sigacation funtion */
    (*sigaction)(node.signum, si, uc);

    /* Resume execution */
    {
        posix_jump_context_t ctx;

        ctx.rip = (uint64_t)uc->uc_mcontext.gregs[REG_RIP];
        ctx.rsp = (uint64_t)uc->uc_mcontext.gregs[REG_RSP];
        ctx.rbp = (uint64_t)uc->uc_mcontext.gregs[REG_RBP];
        ctx.rbx = (uint64_t)uc->uc_mcontext.gregs[REG_RBX];
        ctx.r12 = (uint64_t)uc->uc_mcontext.gregs[REG_R12];
        ctx.r13 = (uint64_t)uc->uc_mcontext.gregs[REG_R13];
        ctx.r14 = (uint64_t)uc->uc_mcontext.gregs[REG_R14];
        ctx.r15 = (uint64_t)uc->uc_mcontext.gregs[REG_R15];

        if (!oe_is_within_enclave((void*)ctx.rip, sizeof(void*)))
            POSIX_PANIC("RIP is outside enclave");

        if (!oe_is_within_enclave((void*)ctx.rsp, sizeof(void*)))
            POSIX_PANIC("RSP is outside enclave");

        if (!oe_is_within_enclave((void*)ctx.rbp, sizeof(void*)))
            POSIX_PANIC("RBP is outside enclave");

        posix_jump(&ctx);
    }
}

extern uint64_t __oe_exception_arg;

//#define USE_CUSTOM_STACK
#ifdef USE_CUSTOM_STACK
#define STACK_SIZE (64*1024)
static __thread void* _tls_stack;
#endif

static uint64_t _exception_handler(oe_exception_record_t* rec)
{
    posix_set_trace(0xf7c4b81e);

    if (__oe_exception_arg == POSIX_SIGACTION)
    {
        posix_shared_block_t* shared_block = posix_shared_block();
        ucontext_t uc;

        // If the exceptoin occurred in the user zone, then dispatch to
        // _user_zone_signal_handler(). Else posix_dispatch_redzone_signals()
        // will directly invoke the user's signal handler upon exiting the
        // redzone.
        if (shared_block->zone == POSIX_ZONE_USER)
        {
            uc.uc_mcontext.gregs[REG_R8] = (int64_t)rec->context->r8;
            uc.uc_mcontext.gregs[REG_R9] = (int64_t)rec->context->r9;
            uc.uc_mcontext.gregs[REG_R10] = (int64_t)rec->context->r10;
            uc.uc_mcontext.gregs[REG_R11] = (int64_t)rec->context->r11;
            uc.uc_mcontext.gregs[REG_R12] = (int64_t)rec->context->r12;
            uc.uc_mcontext.gregs[REG_R13] = (int64_t)rec->context->r13;
            uc.uc_mcontext.gregs[REG_R14] = (int64_t)rec->context->r14;
            uc.uc_mcontext.gregs[REG_R15] = (int64_t)rec->context->r15;
            uc.uc_mcontext.gregs[REG_RDI] = (int64_t)rec->context->rdi;
            uc.uc_mcontext.gregs[REG_RSI] = (int64_t)rec->context->rsi;
            uc.uc_mcontext.gregs[REG_RBP] = (int64_t)rec->context->rbp;
            uc.uc_mcontext.gregs[REG_RBX] = (int64_t)rec->context->rbx;
            uc.uc_mcontext.gregs[REG_RDX] = (int64_t)rec->context->rdx;
            uc.uc_mcontext.gregs[REG_RAX] = (int64_t)rec->context->rax;
            uc.uc_mcontext.gregs[REG_RCX] = (int64_t)rec->context->rcx;
            uc.uc_mcontext.gregs[REG_RSP] = (int64_t)rec->context->rsp;
            uc.uc_mcontext.gregs[REG_RIP] = (int64_t)rec->context->rip;

            _tls_ucontext = uc;

#ifdef USE_CUSTOM_STACK
            {
                if (!_tls_stack)
                {
                    if (!(_tls_stack = oe_memalign(4096, STACK_SIZE)))
                    {
                        POSIX_PANIC("oops");
                    }
                }

                memset(_tls_stack, 0, STACK_SIZE);

                rec->context->rsp = (uint64_t)_tls_stack + (STACK_SIZE / 2);
                rec->context->rbp = (uint64_t)_tls_stack + (STACK_SIZE / 2);
            }
#endif
            rec->context->rip = (uint64_t)_user_zone_signal_handler;
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

    if (POSIX_OCALL(posix_rt_sigaction_ocall(
        &r, signum, act, sigsetsize), 0xa9b209b4) != OE_OK)
    {
        return -EINVAL;
    }

    return r;
}

int posix_rt_sigprocmask(
    int how,
    const sigset_t* set,
    sigset_t* oldset,
    size_t sigsetsize)
{
#if 0
    int retval;

    if (POSIX_OCALL(posix_rt_sigprocmask_ocall(
        &retval,
        how,
        (const struct posix_sigset*)set,
        (struct posix_sigset*)oldset,
        sigsetsize), 0x92c59019) != OE_OK)
    {
        return -EINVAL;
    }

    return retval;
#else
    (void)how;
    (void)set;
    (void)oldset;
    (void)sigsetsize;
    return 0;
#endif
}

int posix_dispatch_redzone_signals(void)
{
    sigaction_handler_t handler = NULL;
    oe_jmpbuf_t env;
    posix_sig_queue_node_t node;

    POSIX_ASSERT(posix_shared_block()->zone == POSIX_ZONE_USER);

    /* Dispatch all entries in the signal queue until empty */
    while (!_sig_queue_empty())
    {
        /* Get the front node from the signal queue */
        if (_sig_queue_pop_front(&node) != 0)
        {
            POSIX_PANIC("signal queue empty");
            return -1;
        }

        /* Get the signal handler from the table */
        {
            posix_spin_lock(&_lock);
            handler = (sigaction_handler_t)_table[node.signum].handler;
            posix_spin_unlock(&_lock);

            if (!handler)
                POSIX_PANIC("handler not found");
        }

        /* Build a ucontext and invoke the signal handler */
        if (oe_setjmp(&env) == 0)
        {
            siginfo_t si;
            ucontext_t uc;

            memcpy(&si, &node.siginfo, sizeof(si));

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
            handler(node.signum, &si, &uc);

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
    }

    return 0;
}

extern struct posix_shared_block* __posix_init_shared_block;

int posix_tkill(int tid, int sig)
{
    int retval;

    if (POSIX_OCALL(posix_tkill_ocall(&retval, tid, sig), 0x8aaf2494) != OE_OK)
        return -ENOSYS;

    return retval;
}
