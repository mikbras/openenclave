// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#ifndef _POSIX_ASSUME_H
#define _POSIX_ASSUME_H

#define POSIX_ASSUME(COND)                                                \
    do                                                                    \
    {                                                                     \
        if (!(COND))                                                      \
            __posix_assume_fail(__FILE__, __LINE__, __FUNCTION__, #COND); \
    }                                                                     \
    while (0)

void __posix_assume_fail(
    const char* file,
    unsigned int line,
    const char* func,
    const char* cond);

#endif /* _POSIX_ASSUME_H */
