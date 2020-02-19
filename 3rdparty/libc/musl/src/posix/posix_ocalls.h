// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#ifndef _POSIX_OCALLS_H
#define _POSIX_OCALLS_H

#include <openenclave/enclave.h>

struct posix_timespec
{
    int64_t tv_sec;
    uint64_t tv_nsec;
};

oe_result_t posix_wait_ocall(
    int* host_uaddr,
    const struct posix_timespec* timeout);

oe_result_t posix_wake_ocall(int* host_uaddr);

oe_result_t posix_wake_wait_ocall(
    int* waiter_host_uaddr,
    int* self_host_uaddr,
    const struct posix_timespec* timeout);

#endif //_POSIX_OCALLS_H
