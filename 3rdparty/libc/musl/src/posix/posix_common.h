#ifndef _POSIX_COMMON_H
#define _POSIX_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define POSIX_INLINE static __inline__

#define POSIX_PRINTF_FORMAT(N, M) __attribute__((format(printf, N, M)))

#endif /* _POSIX_COMMON_H */
