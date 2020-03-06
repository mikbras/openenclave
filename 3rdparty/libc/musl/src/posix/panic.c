#include <openenclave/enclave.h>
#include <openenclave/corelibc/stdio.h>
#include "posix_common.h"
#include "posix_panic.h"
#include "posix_io.h"

void posix_panic(
    const char* file,
    unsigned int line,
    const char* func,
    const char* msg)
{
    posix_printf(
        "enclave: posix_panic: %s(%u): %s(): %s\n", file, line, func, msg);

    oe_abort();
}
