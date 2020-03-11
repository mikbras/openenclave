#include <stdarg.h>
#include <stdio.h>
#include <openenclave/enclave.h>
#include <openenclave/internal/backtrace.h>
#include "posix_panic.h"
#include "posix_ocalls.h"

void __posix_panic(
    const char* file,
    unsigned int line,
    const char* func,
    const char* fmt,
    ...)
{
    uint64_t buffer[OE_BACKTRACE_MAX];
    char msg[1024];
    int size;
    va_list ap;

    if ((size = oe_backtrace((void**)buffer, OE_BACKTRACE_MAX)) < 0)
        size = 0;

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    /* This function never returns */
    posix_panic_ocall(file, line, func, msg, buffer, (size_t)size);
    oe_abort();
}
