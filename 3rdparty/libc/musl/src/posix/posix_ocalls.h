// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#ifndef _POSIX_OCALLS_H
#define _POSIX_OCALLS_H

#include <openenclave/enclave.h>

oe_result_t posix_wait_ocall(int* host_uaddr);

oe_result_t posix_wake_ocall(int* host_uaddr);

oe_result_t posix_wake_wait_ocall(int* waiter_host_uaddr, int* self_host_uaddr);

#endif //_POSIX_OCALLS_H
