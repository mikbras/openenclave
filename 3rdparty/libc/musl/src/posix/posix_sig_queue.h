// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#ifndef _POSIX_SIG_QUEUE_H
#define _POSIX_SIG_QUEUE_H

#include "posix_common.h"
#include "posix_ocall_structs.h"

#define POSIX_SIG_QUEUE_NODE_MAGIC 0x90962d674a93402d

#if 0
#define POSIX_USE_SIG_QUEUE_LOCKING
#endif

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

typedef struct posix_sig_queue
{
    posix_sig_queue_node_t* head;
    posix_sig_queue_node_t* tail;
}
posix_sig_queue_t;

#endif /* _POSIX_SIG_QUEUE_H */
