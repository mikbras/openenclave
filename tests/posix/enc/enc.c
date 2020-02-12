// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#include <openenclave/corelibc/string.h>
#include <openenclave/enclave.h>
#include <openenclave/internal/print.h>
#include "posix_t.h"

void posix_test(void)
{
    oe_host_printf("posix_test()\n");
}

OE_SET_ENCLAVE_SGX(
    1,    /* ProductID */
    1,    /* SecurityVersion */
    true, /* AllowDebug */
    1024, /* HeapPageCount */
    1024, /* StackPageCount */
    2);   /* TCSCount */
