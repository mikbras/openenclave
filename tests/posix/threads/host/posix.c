// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#define _GNU_SOURCE

#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <execinfo.h>
#include <stdarg.h>
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
#include "../../../../3rdparty/libc/musl/src/posix/posix_panic.h"

//#define TRACE
//#define TRACE_THREADS
//#define TRACE_SIGNALS

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

posix_shared_block_t* posix_shared_block(void)
{
    assert(_tls_shared_block != NULL);
    return _tls_shared_block;
}

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

void posix_panic(
    const char* file,
    unsigned int line,
    const char* func,
    const char* msg)
{
    fprintf(stderr, "host: posix_panic: %s(%u): %s(): %s\n",
        file, line, func, msg);
    fflush(stderr);
    abort();
}

int posix_block_all_signals(sigset_t* old_set)
{
    int r;
    sigset_t mask;

    if ((r = sigfillset(&mask)) != 0)
        return r;

    if ((r = sigprocmask(SIG_BLOCK, &mask, old_set)) != 0)
        return r;

    return 0;
}

int posix_restore_signals(sigset_t* set)
{
    return sigprocmask(SIG_SETMASK, set, 0);
}

__attribute__((format(printf, 1, 2)))
__attribute__((used))
static void _trace(const char* fmt, ...)
{
    char buf[1024];
    char prefix[] = "TRACE: ";
    const size_t prefix_len = sizeof(prefix) - 1;
    va_list ap;

#if 0
    sigset_t set;
    posix_block_all_signals(&set);
#endif

    memcpy(buf, prefix, sizeof(prefix));

    va_start(ap, fmt);
    int n = vsnprintf(buf + prefix_len, sizeof(buf) - prefix_len, fmt, ap);
    va_end(ap);

    if (n < 0 || (size_t)n >= sizeof(buf))
    {
        const char msg[] = "TRACE overflowed\n";
        write(STDOUT_FILENO, msg, sizeof(msg));
        abort();
    }

    n += (int)prefix_len;
    strcat(buf, "\n");
    n++;

    write(STDOUT_FILENO, buf, (size_t)n);

#if 0
    posix_restore_signals(&set);
#endif
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
** BEGIN_OCALL/END_OCALL
**
**==============================================================================
*/

#define BEGIN_OCALL _begin_ocall(__FUNCTION__)
#define END_OCALL _end_ocall(__FUNCTION__)

static inline void _begin_ocall(const char* func)
{
#if defined(TRACE)
    _trace("_begin_ocall:%s:%d", func, posix_gettid());
#endif

    if (posix_shared_block()->ocall_lock != 1)
        assert("panic" == NULL);

    if (posix_shared_block()->zone != POSIX_ZONE_OCALL)
        assert("panic" == NULL);

    posix_shared_block()->zone = POSIX_ZONE_HOST;

    posix_spin_unlock(&posix_shared_block()->ocall_lock);
    (void)func;
}

static inline void _end_ocall(const char* func)
{
    (void)func;

    posix_spin_lock(&posix_shared_block()->ocall_lock);

    if (posix_shared_block()->zone != POSIX_ZONE_HOST)
        assert("panic" == NULL);

    posix_shared_block()->zone = POSIX_ZONE_OCALL;

    if (posix_shared_block()->ocall_lock != 1)
        assert("panic" == NULL);

#if defined(TRACE)
    _trace("_end_ocall:%s:%d", func, posix_gettid());
#endif
}

void posix_print_trace(void)
{
    if (_tls_shared_block)
    {
        _trace("posix_print_trace=%08x (%x)", _tls_shared_block->trace,
            posix_gettid());
    }
    else
    {
        _trace("posix_print_trace=null");
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
    _trace("CHILD.START=%d", posix_gettid());
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
        _enclave, &retval, cookie, pthread_self(), tid, shared_block)) != OE_OK)
    {
        assert("posix_run_thread_ecall() failed" == NULL);
    }

    if (retval != 0)
    {
        _trace("posix_run_thread_ecall(): retval=%d", retval);
        abort();
    }

    if (_thread_table_remove(posix_gettid()) != 0)
    {
        assert("_thread_table_remove() failed" == NULL);
    }

    memset(shared_block, 0, sizeof(posix_shared_block_t));
    free(shared_block);
    _tls_shared_block = NULL;

#ifdef TRACE_THREADS
    _trace("CHILD.EXIT=%d", posix_gettid());
#endif

    return NULL;
}

int posix_start_thread_ocall(uint64_t cookie)
{
//#define DETACHED
    BEGIN_OCALL;
    int ret = -1;
    pthread_t t;
    pthread_attr_t* attr = NULL;

#ifdef DETACHED
    pthread_attr_t buf;
    attr = &buf;
    pthread_attr_init(attr);
    pthread_attr_setdetachstate(attr, PTHREAD_CREATE_DETACHED);
#endif

    if (pthread_create(&t, attr, _thread_func, (void*)cookie) != 0)
        goto done;

    ret = 0;

done:

    if (attr)
        pthread_attr_destroy(attr);

    END_OCALL;
    return ret;
}

int posix_gettid_ocall(void)
{
    BEGIN_OCALL;
    int ret = (int)syscall(SYS_gettid);
    END_OCALL;
    return ret;
}

int posix_getpid_ocall(void)
{
    BEGIN_OCALL;
    int ret = (int)syscall(SYS_getpid);
    END_OCALL;
    return ret;
}

int posix_nanosleep_ocall(
    const struct posix_timespec* req,
    struct posix_timespec* rem)
{
    BEGIN_OCALL;
    int ret;

    if ((ret = nanosleep((struct timespec*)req, (struct timespec*)rem)) != 0)
        ret = -errno;

    END_OCALL;
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
    BEGIN_OCALL;
    int retval = 0;

    if (!_valid_uaddr(host_uaddr))
    {
        POSIX_PANIC("invalid uaddr");
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

    END_OCALL;
    return retval;
}

int posix_wake_ocall(int* host_uaddr, int val)
{
    BEGIN_OCALL;
    int retval;

    if (!_valid_uaddr(host_uaddr))
        POSIX_PANIC("invalid uaddr");

    retval = (int)syscall(
        SYS_futex, host_uaddr, FUTEX_WAKE_PRIVATE, val, NULL, NULL, 0);

    if (retval != 0)
        retval = -errno;

    END_OCALL;
    return retval;
}

int posix_futex_requeue_ocall(int* uaddr, int val, int val2, int* uaddr2)
{
    BEGIN_OCALL;
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

    END_OCALL;
    return retval;
}

int posix_clock_gettime_ocall(int clk_id, struct posix_timespec* tp)
{
    BEGIN_OCALL;
    int ret = clock_gettime(clk_id, (struct timespec*)tp);
    END_OCALL;
    return ret;
}

int posix_tkill_ocall(int tid, int sig)
{
    BEGIN_OCALL;
    int ret = -1;
    int retval;

#ifdef TRACE
    _trace("%s(TID=%d, tid=%d, sig=%d)",
        __FUNCTION__, posix_gettid(), tid, sig);
#endif

#if 1
    posix_shared_block_t* shared_block = NULL;
    {

        if (!(shared_block = _thread_table_find(tid)))
            assert("_thread_table_find() failed" == NULL);

        posix_spin_lock(&shared_block->ocall_lock);
    }
#else
    (void)_thread_table_find;
#endif

    retval = (int)syscall(SYS_tkill, tid, sig);

    ret = (retval == 0) ? 0 : -errno;

    END_OCALL;
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

static int _sig_queue_push_back(posix_sig_queue_node_t* node)
{
    int ret = -1;
    posix_shared_block_t* shared_block;
#ifdef POSIX_USE_SIG_QUEUE_LOCKING
    bool locked = false;
#endif

    if (!node || !(shared_block = posix_shared_block()))
        goto done;

#ifdef POSIX_USE_SIG_QUEUE_LOCKING
    posix_spin_lock(&shared_block->sig_queue_lock);
    locked = true;
#endif

#if 1
    /* Free nodes on the free list */
    posix_list_free((posix_list_t*)&shared_block->sig_queue_free_list, free);
#endif

    /* Append node to list */
    posix_list_push_back(
        (posix_list_t*)&shared_block->sig_queue,
        (posix_list_node_t*)node);

    ret = 0;

done:

#ifdef POSIX_USE_SIG_QUEUE_LOCKING
    if (locked)
        posix_spin_unlock(&shared_block->sig_queue_lock);
#endif

    return ret;
}

static posix_sig_queue_node_t* _sig_queue_node_new(
    int signum,
    int is_enclave_signal,
    siginfo_t* siginfo,
    ucontext_t* ucontext)
{
    posix_sig_queue_node_t* node;

    if (!(node = calloc(1, sizeof(posix_sig_queue_node_t))))
        return NULL;

    node->magic = POSIX_SIG_QUEUE_NODE_MAGIC;
    node->signum = signum;
    node->is_enclave_signal = is_enclave_signal,
    node->siginfo = *(struct posix_siginfo*)siginfo;
    node->ucontext = *(struct posix_ucontext*)ucontext;

    return node;
}

static void _posix_host_signal_handler(int sig, siginfo_t* si, ucontext_t* uc)
{
#ifdef TRACE_SIGNALS
    int tid = posix_gettid();
#endif

    posix_spin_unlock(&posix_shared_block()->ocall_lock);

    /* Build the host exception context */
    oe_host_exception_context_t hec = {0};
    hec.rax = (uint64_t)uc->uc_mcontext.gregs[REG_RAX];
    hec.rbx = (uint64_t)uc->uc_mcontext.gregs[REG_RBX];
    hec.rip = (uint64_t)uc->uc_mcontext.gregs[REG_RIP];

    /* If the signal originated within the enclave */
    if (hec.rax == ENCLU_ERESUME && hec.rip == OE_AEP_ADDRESS)
    {
#ifdef TRACE_SIGNALS
        _trace("*** ENCLAVE.SIGNAL: tid=%d sig=%d zone=%d", tid, sig,
            posix_shared_block()->zone);
#endif

        uint64_t action = oe_host_handle_exception(&hec, POSIX_SIGACTION);

        if (action == OE_EXCEPTION_CONTINUE_EXECUTION)
        {
            posix_sig_queue_node_t* node;

#ifdef TRACE_SIGNALS
            _trace("*** ENCLAVE.SIGNAL.CONTINUE: tid=%d sig=%d",
                posix_gettid(), sig);
#endif

            if (!(node = _sig_queue_node_new(sig, 1, si, uc)))
                POSIX_PANIC("_sig_queue_node_new()");

            if (_sig_queue_push_back(node) != 0)
                POSIX_PANIC("_sig_queue_node_new()");

            return;
        }

        POSIX_PANIC("unhanlded signal");
    }
    else
    {
        posix_sig_queue_node_t* node;

#ifdef TRACE_SIGNALS
        _trace("**HOST.SIGNAL: signum=%d tid=%d zone=%d",
            sig, tid, posix_shared_block()->zone);
#endif

        if (!(node = _sig_queue_node_new(sig, 0, si, uc)))
            POSIX_PANIC("_sig_queue_node_new()");

        if (_sig_queue_push_back(node) != 0)
            POSIX_PANIC("_sig_queue_node_new()");

        return;
    }
}

int posix_rt_sigaction_ocall(
    int signum,
    const struct posix_sigaction* pact,
    size_t sigsetsize)
{
    BEGIN_OCALL;
    struct posix_sigaction act = *pact;
    extern void posix_restore(void);

    errno = 0;

    act.handler = (uint64_t)_posix_host_signal_handler;
    act.restorer = (uint64_t)posix_restore;

    long r = syscall(SYS_rt_sigaction, signum, &act, NULL, sigsetsize);

    if (r != 0)
    {
        END_OCALL;
        return -errno;
    }

    END_OCALL;
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
    BEGIN_OCALL;
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
    END_OCALL;
    return r == 0 ? 0 : -errno;
}

void posix_noop_ocall(void)
{
    BEGIN_OCALL;
    END_OCALL;
}

ssize_t posix_write_ocall(int fd, const void* data, size_t size)
{
    BEGIN_OCALL;

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

        fflush(stderr);
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

    END_OCALL;
    return ret;
}

void posix_raw_puts_ocall(const char* str)
{
    fprintf(stdout, "%s", str);
    fflush(stdout);
}

int posix_join_ocall(uint64_t host_pthread)
{
    return pthread_join((pthread_t)host_pthread, NULL);
}

void posix_assume_ocall(
    const char* file,
    uint32_t line,
    const char* func,
    const char* cond)
{
    fprintf(stderr, "assumption failed: %s(%u): %s(): %s\n",
        file, line, func, cond);
}

void posix_panic_ocall(
    const char* file,
    uint32_t line,
    const char* func,
    const char* cond)
{
    fprintf(stderr, "panic: %s(%u): %s(): %s\n", file, line, func, cond);
}
