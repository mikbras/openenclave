#include <openenclave/enclave.h>
#include "posix_assert.h"
#include "posix_io.h"
#include "posix_trace.h"

void __posix_assert_fail(
    const char* file,
    unsigned int line,
    const char* func,
    const char* cond)
{
    posix_printf("posix assert failed: %s(%u): %s(): %s\n",
        file, line, func, cond);

    posix_print_backtrace();
    oe_abort();
}
