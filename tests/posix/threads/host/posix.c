// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#define _GNU_SOURCE

#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
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

#define POSIX_STRUCT(PREFIX,NAME) OE_CONCAT(t_,NAME)
#include "../../../../3rdparty/libc/musl/src/posix/posix_common.h"
#include "../../../../3rdparty/libc/musl/src/posix/posix_structs.h"
#include "../../../../3rdparty/libc/musl/src/posix/posix_ocall_structs.h"
#include "../../../../3rdparty/libc/musl/src/posix/posix_spinlock.h"

/* ATTN: single enclave instance */
static oe_enclave_t* _enclave = NULL;

static volatile int _uaddrs[1024];
static const size_t _num_uaddrs = OE_COUNTOF(_uaddrs);

/*
**==============================================================================
**
** Verify consistent sizes of structs:
**
**==============================================================================
*/

OE_STATIC_ASSERT(sizeof(struct posix_timespec) == sizeof(struct t_timespec));

OE_STATIC_ASSERT(sizeof(struct posix_sigaction) == sizeof(struct t_sigaction));

OE_STATIC_ASSERT( sizeof(struct posix_siginfo) == sizeof(struct t_siginfo));

OE_STATIC_ASSERT(sizeof(struct posix_ucontext) == sizeof(struct t_ucontext));

OE_STATIC_ASSERT(sizeof(struct posix_sigset) == sizeof(struct t_sigset));

__thread struct posix_shared_block* _tls_shared_block = NULL;

/*
**==============================================================================
**
** Utility functions:
**
**==============================================================================
*/

int posix_gettid(void)
{
    return (int)syscall(SYS_gettid);
}

void sleep_msec(uint64_t milliseconds)
{
    struct timespec ts;
    const struct timespec* req = &ts;
    struct timespec rem = {0, 0};
    static const uint64_t _SEC_TO_MSEC = 1000UL;
    static const uint64_t _MSEC_TO_NSEC = 1000000UL;

    ts.tv_sec = (time_t)(milliseconds / _SEC_TO_MSEC);
    ts.tv_nsec = (long)((milliseconds % _SEC_TO_MSEC) * _MSEC_TO_NSEC);

    while (nanosleep(req, &rem) != 0 && errno == EINTR)
    {
        req = &rem;
    }
}

/*
**==============================================================================
**
** The thread table:
**
**==============================================================================
*/

typedef struct _thread_table_entry
{
    int tid;
    struct posix_shared_block* shared_block;
}
thread_table_entry_t;

static thread_table_entry_t _thread_table[128];
static const size_t THREAD_TABLE_SIZE = OE_COUNTOF(_thread_table);
static size_t _thread_table_size;
static posix_spinlock_t _thread_table_lock;

static int _thread_table_add(int tid, posix_shared_block_t* shared_block)
{
    int ret = -1;

    posix_spin_lock(&_thread_table_lock);

    if (tid < 0 || !shared_block)
        goto done;

    if (_thread_table_size == THREAD_TABLE_SIZE)
        goto done;

    _thread_table[_thread_table_size].tid = tid;
    _thread_table[_thread_table_size].shared_block = shared_block;
    _thread_table_size++;

    ret = 0;

done:

    if (ret != 0)
        assert(__FUNCTION__ == NULL);

    posix_spin_unlock(&_thread_table_lock);
    return ret;
}

static int _thread_table_remove(int tid)
{
    int ret = -1;
    bool found = false;

    posix_spin_lock(&_thread_table_lock);

    for (size_t i = 0; i < _thread_table_size; i++)
    {
        if (_thread_table[i].tid == tid)
        {
            found = true;
            _thread_table[i] = _thread_table[_thread_table_size - 1];
            _thread_table_size--;
            break;
        }
    }

    if (!found)
        goto done;

    ret = 0;

done:

    if (ret != 0)
        assert(__FUNCTION__ == NULL);

    posix_spin_unlock(&_thread_table_lock);
    return ret;
}

static posix_shared_block_t* _thread_table_find(int tid)
{
    posix_shared_block_t* ret = NULL;

    posix_spin_lock(&_thread_table_lock);

    for (size_t i = 0; i < _thread_table_size; i++)
    {
        if (_thread_table[i].tid == tid)
        {
            ret = _thread_table[i].shared_block;
            break;
        }
    }

    posix_spin_unlock(&_thread_table_lock);

    if (!ret)
        assert(__FUNCTION__ == NULL);

    return ret;
}

/*
**==============================================================================
**
** signal locks:
**
**==============================================================================
*/

void posix_print_signal_lock(void)
{
    uint32_t val = _tls_shared_block->signal_lock;
    printf("posix_print_signal_lock: val=%u\n", val);
}

void posix_lock_signal_by_tid(int tid)
{
#ifdef USE_SIGNAL_LOCK
    posix_shared_block_t* shared_block;

    if (!(shared_block = _thread_table_find(tid)))
        assert("_thread_table_find() failed" == NULL);

    posix_spin_lock(&shared_block->signal_lock);
#endif
}

void posix_unlock_signal_by_tid(int tid)
{
#ifdef USE_SIGNAL_LOCK
    posix_shared_block_t* shared_block;

    if (!(shared_block = _thread_table_find(tid)))
        assert("_thread_table_find() failed" == NULL);

    posix_spin_unlock(&shared_block->signal_lock);
#endif
}

void posix_lock_signal(void)
{
#ifdef USE_SIGNAL_LOCK
    assert(_tls_shared_block != NULL);
    if (_tls_shared_block)
        posix_spin_lock(&_tls_shared_block->signal_lock);
#endif
}

void posix_unlock_signal(void)
{
#ifdef USE_SIGNAL_LOCK
    assert(_tls_shared_block != NULL);
    if (_tls_shared_block)
        posix_spin_unlock(&_tls_shared_block->signal_lock);
#endif
}

/*
**==============================================================================
**
** ENTER/LEAVE
**
**==============================================================================
*/

//#define TRACE
#define ENTER __enter(__FUNCTION__, true)
#define LEAVE __leave(__FUNCTION__, true)

#define ENTER_NON_LOCKING __enter(__FUNCTION__, false)
#define LEAVE_NON_LOCKING __leave(__FUNCTION__, false)

static inline void __enter(const char* func, bool unlock)
{
    (void)func;

    if (unlock)
        posix_unlock_signal();

#if defined(TRACE)
    printf("__enter:%s:%d\n", func, posix_gettid());
    fflush(stdout);
#endif
}

static inline void __leave(const char* func, bool lock)
{
    (void)func;

#if defined(TRACE)
    printf("__leave:%s:%d\n", func, posix_gettid());
    fflush(stdout);
#endif

    if (lock)
        posix_lock_signal();
}

void posix_print_trace(void)
{
    if (_tls_shared_block)
    {
        printf("*** posix_print_trace=%d (%d)\n", _tls_shared_block->trace,
            posix_gettid());
        fflush(stdout);
    }
    else
    {
        printf("*** posix_print_trace=null\n");
        fflush(stdout);
    }
}

/*
**==============================================================================
**
**
**
**==============================================================================
*/

extern int posix_init(
    oe_enclave_t* enclave,
    posix_shared_block_t** shared_block_out)
{
    int ret = -1;
    posix_shared_block_t* shared_block = NULL;

    if (shared_block_out)
        *shared_block_out = NULL;

    if (!enclave || !shared_block_out)
        goto done;

    if (!(shared_block = calloc(1, sizeof(struct posix_shared_block))))
        goto done;

    shared_block->uaddrs = _uaddrs;
    shared_block->num_uaddrs = _num_uaddrs;

    if (_thread_table_add(posix_gettid(), shared_block) != 0)
        goto done;

    _tls_shared_block = shared_block;
    *shared_block_out = shared_block;
    shared_block = NULL;

    /* ATTN: single instance for now */
    _enclave = enclave;

    ret = 0;

done:

    if (shared_block)
        free(shared_block);

    return ret;
}

static void* _thread_func(void* arg)
{
    int retval;
    oe_result_t r;
    uint64_t cookie = (uint64_t)arg;
    posix_shared_block_t* shared_block;

#ifdef TRACE_THREADS
    printf("CHILD.START=%d\n", posix_gettid());
    fflush(stdout);
#endif

    assert(_tls_shared_block == NULL);

    /* Allocate the block shared between the host and enclave */
    {
        if (!(shared_block = calloc(1, sizeof(posix_shared_block_t))))
        {
            assert("calloc() failed" == NULL);
        }

        shared_block->uaddrs = _uaddrs;
        shared_block->num_uaddrs = _num_uaddrs;

    }

    _tls_shared_block = shared_block;

    if (_thread_table_add(posix_gettid(), shared_block) != 0)
    {
        assert("_thread_table_add() failed" == NULL);
    }

    int tid = posix_gettid();

    if ((r = posix_run_thread_ecall(
        _enclave, &retval, cookie, tid, shared_block)) != OE_OK)
    {
        assert("posix_run_thread_ecall() failed" == NULL);
    }

    if (retval != 0)
    {
        fprintf(stderr, "posix_run_thread_ecall(): retval=%d\n", retval);
        abort();
    }


#ifdef TRACE_THREADS
    printf("CHILD.EXIT=%d\n", posix_gettid());
    fflush(stdout);
#endif

    if (_thread_table_remove(posix_gettid()) != 0)
    {
        assert("_thread_table_remove() failed" == NULL);
    }

    free(shared_block);
    _tls_shared_block = NULL;

    return NULL;
}

int posix_start_thread_ocall(uint64_t cookie)
{
    ENTER;
    int ret = -1;
    pthread_t t;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (pthread_create(&t, &attr, _thread_func, (void*)cookie) != 0)
        goto done;

    ret = 0;

done:
    pthread_attr_destroy(&attr);
    LEAVE;
    return ret;
}

int posix_gettid_ocall(void)
{
    ENTER;
    int ret = (int)syscall(SYS_gettid);
    LEAVE;
    return ret;
}

int posix_getpid_ocall(void)
{
    ENTER;
    int ret = (int)syscall(SYS_getpid);
    LEAVE;
    return ret;
}

int posix_nanosleep_ocall(
    const struct posix_timespec* req,
    struct posix_timespec* rem)
{
    ENTER;
    int ret;

    if ((ret = nanosleep((struct timespec*)req, (struct timespec*)rem)) != 0)
        ret = -errno;

    LEAVE;
    return ret;
}

static bool _valid_uaddr(const int* uaddr)
{
    assert(_tls_shared_block != NULL);
    assert(_tls_shared_block->uaddrs == _uaddrs);
    assert(_tls_shared_block->num_uaddrs == _num_uaddrs);

    volatile int* start = _uaddrs;
    volatile int* end = &_uaddrs[_num_uaddrs];

    return uaddr >= start && uaddr < end;
}

int posix_wait_ocall(
    int* host_uaddr,
    int val,
    const struct posix_timespec* timeout)
{
    ENTER;
    int retval = 0;

    if (!_valid_uaddr(host_uaddr))
    {
        printf("invalid uaddr: %p\n", host_uaddr);
        assert("invalid uaddr" == NULL);
        abort();
    }

    for (;;)
    {
        retval = (int)syscall(
            SYS_futex,
            host_uaddr,
            FUTEX_WAIT_PRIVATE,
            val,
            timeout,
            NULL,
            0);

        if (retval == 0)
            break;

break;

        if (errno != EAGAIN)
            break;
    }

    if (retval != 0)
        retval = -errno;

#if 0
    if (retval != 0 && retval != -ETIMEDOUT)
    {
        printf("futex wait failed: retval=%d\n", retval);
        assert("futex wait failed" == NULL);
        abort();
    }
#endif

    LEAVE;
    return retval;
}

int posix_wake_ocall(int* host_uaddr, int val)
{
    ENTER;
    int retval;

    if (!_valid_uaddr(host_uaddr))
    {
        printf("invalid uaddr: %p\n", host_uaddr);
        assert("invalid uaddr" == NULL);
        abort();
    }

    retval = (int)syscall(
        SYS_futex, host_uaddr, FUTEX_WAKE_PRIVATE, val, NULL, NULL, 0);

    if (retval != 0)
        retval = -errno;

    LEAVE;
    return retval;
}

int posix_futex_requeue_ocall(int* uaddr, int val, int val2, int* uaddr2)
{
    ENTER;
    int retval;

    if (!_valid_uaddr(uaddr))
    {
        assert("invalid uaddr" == NULL);
        abort();
    }

    if (!_valid_uaddr(uaddr2))
    {
        assert("invalid uaddr2" == NULL);
        abort();
    }

    retval = (int)syscall(
        SYS_futex, uaddr, FUTEX_REQUEUE_PRIVATE, val, val2, uaddr2, 0);

    if (retval != 0)
        retval = -errno;

    LEAVE;
    return retval;
}

int posix_clock_gettime_ocall(int clk_id, struct posix_timespec* tp)
{
    ENTER;
    int ret = clock_gettime(clk_id, (struct timespec*)tp);
    LEAVE;
    return ret;
}

int posix_tkill_ocall(int tid, int sig)
{
    ENTER_NON_LOCKING;
    int ret = -1;
    int retval;

#if 1
    printf("%s(TID=%d, tid=%d, sig=%d)\n",
        __FUNCTION__, posix_gettid(), tid, sig);
    fflush(stdout);
#endif

    posix_lock_signal_by_tid(tid);
    retval = (int)syscall(SYS_tkill, tid, sig);
    //posix_unlock_signal_by_tid(tid);

    ret = (retval == 0) ? 0 : -errno;

    LEAVE_NON_LOCKING;
    return ret;
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

static void _set_sig_args(
    int enclave_sig,
    int sig,
    siginfo_t* si,
    ucontext_t* uc)
{
    struct posix_sig_args* args = &_tls_shared_block->sig_args;

    memset(args, 0, sizeof(struct posix_sig_args));
    args->enclave_sig = enclave_sig,
    args->sig = sig;
    args->siginfo = *(struct posix_siginfo*)si;
    args->ucontext = *(struct posix_ucontext*)uc;
}

static void _posix_host_signal_handler(int sig, siginfo_t* si, ucontext_t* uc)
{
    posix_unlock_signal();

    /* Build the host exception context */
    oe_host_exception_context_t hec = {0};
    hec.rax = (uint64_t)uc->uc_mcontext.gregs[REG_RAX];
    hec.rbx = (uint64_t)uc->uc_mcontext.gregs[REG_RBX];
    hec.rip = (uint64_t)uc->uc_mcontext.gregs[REG_RIP];

    /* If the signal originated within the enclave */
    if (hec.rax == ENCLU_ERESUME && hec.rip == OE_AEP_ADDRESS)
    {
#if 1
        fprintf(stderr, "*** ENCLAVE.SIGNAL: tid=%d sig=%d\n",
            posix_gettid(), sig);
        fflush(stderr);
#endif

        uint64_t action = oe_host_handle_exception(&hec, POSIX_SIGACTION);

        if (action == OE_EXCEPTION_CONTINUE_EXECUTION)
        {
            fprintf(stderr, "*** ENCLAVE.SIGNAL.CONTINUE: tid=%d sig=%d\n",
                posix_gettid(), sig);
            fflush(stderr);
            _set_sig_args(true, sig, si, uc);
            return;
        }

        fprintf(stderr, "unhanlded signal: %d\n", sig); fflush(stderr);
        abort();
    }
    else
    {
#if 1
        fprintf(stderr, "*** HOST.SIGNAL: tid=%d sig=%d\n",
            posix_gettid(), sig);
        fflush(stderr);
#endif

        _set_sig_args(false, sig, si, uc);
        return;
    }
}

int posix_rt_sigaction_ocall(
    int signum,
    const struct posix_sigaction* pact,
    size_t sigsetsize)
{
    ENTER;
    struct posix_sigaction act = *pact;
    extern void posix_restore(void);

#if 0
    printf("%s(tid=%d, signum=%d)\n",
        __FUNCTION__, posix_gettid(), signum);
    fflush(stdout);
#endif

    errno = 0;

    act.handler = (uint64_t)_posix_host_signal_handler;
    act.restorer = (uint64_t)posix_restore;

    long r = syscall(SYS_rt_sigaction, signum, &act, NULL, sigsetsize);

    if (r != 0)
    {
        LEAVE;
        return -errno;
    }

    LEAVE;
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
    ENTER;
#if 0
    const char* howstr;

    switch (how)
    {
        case SIG_BLOCK:
            howstr = "block";
            break;
        case SIG_UNBLOCK:
            howstr = "unblock";
            break;
        case SIG_SETMASK:
            howstr = "setmask";
            break;
        default:
            howstr = "unknown";
            break;
    }

    printf("posix_rt_sigprocmask_ocall(how=%s, tid=%d)\n",
        howstr, posix_gettid());

    posix_dump_sigset(howstr, set);
#endif

    long r = syscall(SYS_rt_sigprocmask, how, set, oldset, sigsetsize);
    LEAVE;
    return r == 0 ? 0 : -errno;
}

void posix_noop_ocall(void)
{
    ENTER;
    LEAVE;
}

ssize_t posix_write_ocall(int fd, const void* data, size_t size)
{
    ENTER;
    ssize_t ret = -1;

    if (size == 0)
    {
        ret = 0;
        goto done;
    }

    if (fd == STDOUT_FILENO)
    {
        if (fwrite(data, 1, size, stdout) != size)
            goto done;

        fflush(stdout);
        ret = (ssize_t)size;
    }
    else if (fd == STDERR_FILENO)
    {
        if (fwrite(data, 1, size, stderr) != size)
            goto done;

        fflush(stderr);
        ret = (ssize_t)size;
    }
    else
    {
        assert("posix_write_ocall() panic" == NULL);
    }

done:

    LEAVE;
    return ret;
}
