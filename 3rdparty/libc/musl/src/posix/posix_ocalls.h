// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#ifndef _POSIX_OCALLS_H
#define _POSIX_OCALLS_H

#include <openenclave/bits/types.h>
#include <openenclave/bits/result.h>

typedef struct posix_timespec posix_timespec_t;
typedef struct posix_sigaction posix_sigaction_t;
typedef struct posix_siginfo posix_siginfo_t;
typedef struct posix_ucontext posix_ucontext_t;

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
    const struct posix_timespec* timeout);

oe_result_t posix_wake_ocall(int* host_uaddr);

oe_result_t posix_wake_wait_ocall(
    int* retval,
    int* waiter_host_uaddr,
    int* self_host_uaddr,
    const struct posix_timespec* timeout);

oe_result_t posix_start_thread_ocall(int* retval, uint64_t cookie);

oe_result_t posix_gettid_ocall(int* retval);

oe_result_t posix_tkill_ocall(int* retval, int tid, int sig);

oe_result_t posix_rt_sigaction_ocall(
    int* retval,
    int signum,
    const struct posix_sigaction* act,
    struct posix_sigaction* oldact,
    size_t sigsetsize);

oe_result_t posix_get_sigaction_args_ocall(
    int* retval,
    int* sig,
    struct posix_siginfo* siginfo,
    struct posix_ucontext* ucontext);

#endif //_POSIX_OCALLS_H
