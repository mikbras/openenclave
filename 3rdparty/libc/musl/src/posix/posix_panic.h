// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#ifndef _POSIX_PANIC_H
#define _POSIX_PANIC_H

#define POSIX_PANIC __posix_panic(__FILE__, __LINE__, __FUNCTION__, NULL)

#define POSIX_PANIC_MSG(MSG) \
    __posix_panic(__FILE__, __LINE__, __FUNCTION__, MSG)

void __posix_panic(
    const char* file,
    unsigned int line,
    const char* func,
    const char* msg);

#endif /* _POSIX_PANIC_H */
