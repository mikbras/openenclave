// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#ifndef _POSIX_SIGNAL_H
#define _POSIX_SIGNAL_H

#include <stdint.h>
#include "posix_ocalls.h"

struct posix_sigaction
{
    uint64_t handler;
    unsigned long flags;
    uint64_t restorer;
    unsigned mask[2];
};

void __posix_install_exception_handler(void);

int posix_rt_sigaction(
    int signum,
    const struct posix_sigaction* act,
    struct posix_sigaction* oldact,
    size_t sigsetsize);

#endif //_POSIX_SIGNAL_H
