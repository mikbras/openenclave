// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#ifndef _POSIX_SHARED_BLOCK_H
#define _POSIX_SHARED_BLOCK_H

#include "posix_common.h"
#include "posix_sig_queue.h"
#include "posix_spinlock.h"

#define POSIX_GREENZONE 0x00000000
#define POSIX_REDZONE 0xd661779d

typedef struct posix_shared_block
{
    int32_t futex;
    uint32_t trace;
    volatile int* uaddrs;
    size_t num_uaddrs;
    posix_sig_queue_t sig_queue;
    posix_sig_queue_t sig_queue_free_list;
#ifdef POSIX_USE_SIG_QUEUE_LOCKING
    posix_spinlock_t sig_queue_lock;
#endif
    posix_spinlock_t ocall_lock;

    /* Which zone this thread is currently running in. */
    uint32_t zone;
}
posix_shared_block_t;

#endif /* _POSIX_SHARED_BLOCK_H */
