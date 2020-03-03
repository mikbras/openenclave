// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#ifndef _POSIX_OCALLS_SHARED_BLOCK_H
#define _POSIX_OCALLS_SHARED_BLOCK_H

#include <openenclave/bits/types.h>
#include <openenclave/bits/result.h>
#include <openenclave/internal/defs.h>
#include "posix_ocall_structs.h"

#ifndef POSIX_STRUCT
#define POSIX_STRUCT(PREFIX, STRUCT) OE_CONCAT(PREFIX, STRUCT)
#endif

struct POSIX_STRUCT(posix_,timespec)
{
    int64_t tv_sec;
    uint64_t tv_nsec;
};

struct POSIX_STRUCT(posix_,sigaction)
{
    uint64_t handler;
    unsigned long flags;
    uint64_t restorer;
    unsigned mask[2];
};

struct POSIX_STRUCT(posix_,siginfo)
{
    uint8_t data[128];
};

struct POSIX_STRUCT(posix_,ucontext)
{
    uint8_t data[936];
};

struct POSIX_STRUCT(posix_,sigset)
{
    unsigned long __bits[16];
};

#endif //_POSIX_OCALLS_SHARED_BLOCK_H
