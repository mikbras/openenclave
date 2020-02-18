// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#ifndef _POSIX_COND_H
#define _POSIX_COND_H

#include "posix_thread.h"
#include "posix_spinlock.h"

typedef struct _posix_cond
{
    posix_spinlock_t lock;
    posix_thread_queue_t queue;
}
posix_cond_t;

#endif //_POSIX_COND_H
