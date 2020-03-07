// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#ifndef _POSIX_ASSERT_H
#define _POSIX_ASSERT_H

#define POSIX_ASSERT(COND)                                               \
    do                                                                   \
    {                                                                    \
        if (COND)                                                        \
            __posix_assert_fail(__FILE__, __LINE__, __FUNCTION__, #COND) \
    }                                                                    \
    while (0)

void __posix_assert_fail(
    const char* file,
    unsigned int line,
    const char* func,
    const char* cond);

#endif /* _POSIX_ASSERT_H */
