#include <openenclave/enclave.h>
#include <openenclave/internal/backtrace.h>
#include "posix_assume.h"
#include "posix_ocalls.h"

void __posix_assume(
    const char* file,
    unsigned int line,
    const char* func,
    const char* cond)
{
    uint64_t buffer[OE_BACKTRACE_MAX];
    int size;

    if ((size = oe_backtrace((void**)buffer, OE_BACKTRACE_MAX)) < 0)
        size = 0;

    /* This function never returns */
    posix_assume_ocall(file, line, func, cond, buffer, (size_t)size);
    oe_abort();
}
