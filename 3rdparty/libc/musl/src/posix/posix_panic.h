// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#ifndef _POSIX_PANIC_H
#define _POSIX_PANIC_H

#include "posix_io.h"

#define POSIX_PANIC                            \
    do                                         \
    {                                          \
        posix_printf("%s(%u): %s(): panic\n",  \
            __FILE__, __LINE__, __FUNCTION__); \
        oe_abort();                            \
    }                                          \
    while (0)

#endif //_POSIX_PANIC_H
