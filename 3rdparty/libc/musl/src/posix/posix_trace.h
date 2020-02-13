#ifndef _POSIX_TRACE_H
#define _POSIX_TRACE_H

#include "posix_io.h"

#define TRACE \
    posix_printf("TRACE: %s(%u): %s()\n", __FILE__, __LINE__, __FUNCTION__)

#endif /* _POSIX_TRACE_H */
