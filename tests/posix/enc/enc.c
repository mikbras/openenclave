// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#include <openenclave/corelibc/string.h>
#include <openenclave/enclave.h>
#include <stdio.h>
#include <openenclave/internal/print.h>
#include "posix_t.h"

void posix_test(void)
{
    void posix_init_libc(void);
    posix_init_libc();

    printf("=== posix_test()\n");
}

void oe_verify_report_ecall()
{
}

void oe_get_public_key_ecall()
{
}

void oe_get_public_key_by_policy_ecall()
{
}

OE_SET_ENCLAVE_SGX(
    1,    /* ProductID */
    1,    /* SecurityVersion */
    true, /* AllowDebug */
    1024, /* HeapPageCount */
    1024, /* StackPageCount */
    2);   /* TCSCount */
