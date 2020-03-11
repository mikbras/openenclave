// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#ifndef _POSIX_OCALLS_H
#define _POSIX_OCALLS_H

#include <openenclave/bits/types.h>
#include <openenclave/bits/result.h>
#include "posix_common.h"

#define POSIX_OCALL(EXPR, ID)  \
    ({                         \
        oe_result_t __r;       \
        posix_begin_ocall(ID); \
        __r = EXPR;            \
        posix_end_ocall(ID);   \
        __r;                   \
    })

typedef struct posix_timespec posix_timespec_t;
typedef struct posix_sigaction posix_sigaction_t;
typedef struct posix_siginfo posix_siginfo_t;
typedef struct posix_ucontext posix_ucontext_t;
typedef struct posix_sigset posix_sigset_t;
typedef struct posix_sig_args posix_sig_args_t;

void posix_begin_ocall(uint32_t lock_id);

void posix_end_ocall(uint32_t lock_id);

oe_result_t posix_nanosleep_ocall(
    int* retval,
    const struct posix_timespec* req,
    struct posix_timespec* rem);

oe_result_t posix_clock_gettime_ocall(
    int* retval,
    int clk_id,
    struct posix_timespec* tp);

oe_result_t posix_wait_ocall(
    int* retval,
    int* host_uaddr,
    int val,
    const struct posix_timespec* timeout);

oe_result_t posix_wake_ocall(int* retval, int* host_uaddr, int val);

oe_result_t posix_futex_requeue_ocall(
    int* retval,
    int* uaddr,
    int val,
    int val2,
    int* uaddr2);

oe_result_t posix_start_thread_ocall(int* retval, uint64_t cookie);

oe_result_t posix_gettid_ocall(int* retval);

oe_result_t posix_getpid_ocall(int* retval);

oe_result_t posix_tkill_syscall_ocall(long* retval, int tid, int sig);

oe_result_t posix_rt_sigaction_syscall_ocall(
    long* retval,
    int signum,
    const struct posix_sigaction* act,
    size_t sigsetsize);

oe_result_t posix_rt_sigprocmask_syscall_ocall(
    long* retval,
    int how,
    const struct posix_sigset* set,
    struct posix_sigset* oldset,
    size_t sigsetsize);

oe_result_t posix_noop_ocall(void);

oe_result_t posix_write_ocall(
    ssize_t* retval,
    int fd,
    const void* data,
    size_t size);

oe_result_t posix_assume_ocall(
    const char* file,
    uint32_t line,
    const char* func,
    const char* cond,
    const uint64_t* backtrace,
    size_t backtrace_count);

oe_result_t posix_panic_ocall(
    const char* file,
    uint32_t line,
    const char* func,
    const char* msg,
    const uint64_t* backtrace,
    size_t backtrace_count);

#endif /* _POSIX_OCALLS_H */
