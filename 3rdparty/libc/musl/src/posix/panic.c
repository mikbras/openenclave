#include <openenclave/enclave.h>
#include "posix_panic.h"
#include "posix_ocalls.h"

void __posix_panic(
    const char* file,
    unsigned int line,
    const char* func,
    const char* msg)
{
    /* This function never returns */
    posix_panic_ocall(file, line, func, msg);
    oe_abort();
}
