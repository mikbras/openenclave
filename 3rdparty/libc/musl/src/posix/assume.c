#include <openenclave/enclave.h>
#include "posix_assume.h"
#include "posix_ocalls.h"

void __posix_assume(
    const char* file,
    unsigned int line,
    const char* func,
    const char* cond)
{
    /* This function never returns */
    posix_assume_ocall(file, line, func, cond);
    oe_abort();
}
