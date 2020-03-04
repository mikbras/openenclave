// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#ifndef _POSIX_OCALLS_OCALL_STRUCTS_H
#define _POSIX_OCALLS_OCALL_STRUCTS_H

#include <openenclave/bits/types.h>
#include <openenclave/bits/result.h>
#include <openenclave/internal/defs.h>
#include "posix_ocall_structs.h"

typedef struct posix_shared_block posix_shared_block_t;
typedef struct posix_sig_args posix_sig_args_t;

struct posix_sig_args
{
    int sig;
    int enclave_sig;
    struct posix_siginfo siginfo;
    struct posix_ucontext ucontext;
};

struct posix_shared_block
{
    struct posix_sig_args sig_args;
    int32_t futex;
    uint32_t trace;
    volatile uint32_t signal_lock;
    uint32_t padding;
    volatile int* uaddrs;
    size_t num_uaddrs;
};

#endif //_POSIX_OCALLS_OCALL_STRUCTS_H
