// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#ifndef _POSIX_SIGNAL_H
#define _POSIX_SIGNAL_H

#include "posix_common.h"

#define POSIX_SIGACTION 0x515d906d058a5252

typedef struct posix_sigaction posix_sigaction_t;
typedef struct posix_sigset posix_sigset_t;

void __posix_install_exception_handler(void);

int posix_dispatch_redzone_signals(void);

long posix_rt_sigaction_syscall(
    int signum,
    const posix_sigaction_t* act,
    posix_sigaction_t* oldact,
    size_t sigsetsize);

long posix_rt_sigprocmask_syscall(
    int how,
    const posix_sigset_t* set,
    posix_sigset_t* oldset,
    size_t sigsetsize);

#endif /* _POSIX_SIGNAL_H */
