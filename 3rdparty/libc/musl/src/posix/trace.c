#include <openenclave/enclave.h>
#include <openenclave/internal/backtrace.h>
#include "posix_trace.h"

#include "posix_warnings.h"

void posix_print_backtrace(void)
{
    oe_print_backtrace();
}
