// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#ifndef _POSIX_SIGNAL_H
#define _POSIX_SIGNAL_H

#include <stdint.h>
#include <signal.h>

#define POSIX_SIGACTION 0x515d906d058a5252

typedef struct posix_sigaction posix_sigaction_t;
typedef struct posix_sigaction_args posix_sigaction_args_t;

void __posix_install_exception_handler(void);

int posix_rt_sigaction(
    int signum,
    const struct posix_sigaction* act,
    struct posix_sigaction* oldact,
    size_t sigsetsize);

int posix_rt_sigprocmask(
    int how,
    const sigset_t* set,
    sigset_t* oldset,
    size_t sigsetsize);

int posix_dispatch_signal(posix_sigaction_args_t* args);

#endif //_POSIX_SIGNAL_H
