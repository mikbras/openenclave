// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#ifndef _POSIX_STRUCTS_H
#define _POSIX_STRUCTS_H

#include <openenclave/bits/types.h>
#include <openenclave/bits/result.h>
#include <openenclave/internal/defs.h>
#include "posix_ocall_structs.h"
#include "posix_list.h"
#include "posix_spinlock.h"

#define POSIX_SIG_QUEUE_NODE_MAGIC 0x90962d674a93402d

typedef struct posix_sig_queue_node posix_sig_queue_node_t;
struct posix_sig_queue_node
{
    /* Must align with first two fields of posix_list_node_t */
    posix_sig_queue_node_t* next;
    posix_sig_queue_node_t* prev;

    uint64_t magic;

    /* The signal number passed to the host-side signal handler */
    int signum;

    /* Non-zero if the signal interrupted the enclave */
    int is_enclave_signal;

    /* Signal information from the host signal handler */
    struct posix_siginfo siginfo;

    /* The ucontext from the host signal handler */
    struct posix_ucontext ucontext;
};

typedef struct posix_sig_queue posix_sig_queue_t;
struct posix_sig_queue
{
    posix_sig_queue_node_t* head;
    posix_sig_queue_node_t* tail;
};

typedef struct posix_shared_block posix_shared_block_t;
struct posix_shared_block
{
    int32_t futex;
    uint32_t trace;
    volatile uint32_t signal_lock;
    uint32_t signal_lock_id;
    uint32_t padding;
    volatile int* uaddrs;
    size_t num_uaddrs;
    posix_sig_queue_t sig_queue;
    posix_sig_queue_t sig_queue_free_list;
    posix_spinlock_t sig_queue_lock;
};

#endif /* _POSIX_STRUCTS_H */
