// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#ifndef _POSIX_OCALLS_OCALL_TYPES_H
#define _POSIX_OCALLS_OCALL_TYPES_H

#include <openenclave/bits/types.h>
#include <openenclave/bits/result.h>

struct posix_sigaction
{
    uint64_t handler;
    unsigned long flags;
    uint64_t restorer;
    unsigned mask[2];
};

struct posix_sigset
{
    unsigned long __bits[128/sizeof(long)];
};

struct posix_siginfo
{
    uint8_t data[128];
};

struct posix_ucontext
{
    uint8_t data[936];
};

struct posix_sigaction_args
{
    int sig;
    struct posix_siginfo siginfo;
    struct posix_ucontext ucontext;
};

#endif //_POSIX_OCALLS_OCALL_TYPES_H
