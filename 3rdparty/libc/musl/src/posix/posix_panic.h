// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#ifndef _POSIX_PANIC_H
#define _POSIX_PANIC_H

#include "posix_common.h"

#define POSIX_PANIC __posix_panic(__FILE__, __LINE__, __FUNCTION__, NULL)

#define POSIX_PANIC_MSG(FMT, ...) \
    __posix_panic(__FILE__, __LINE__, __FUNCTION__, FMT, ##__VA_ARGS__)

POSIX_PRINTF_FORMAT(4, 5)
void __posix_panic(
    const char* file,
    unsigned int line,
    const char* func,
    const char* fmt,
    ...);

#endif /* _POSIX_PANIC_H */
