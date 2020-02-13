// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#include <openenclave/corelibc/string.h>
#include <openenclave/enclave.h>
#include <stdio.h>
#include <openenclave/internal/print.h>
#include <pthread.h>
#include "posix_t.h"

void* thread(void* arg)
{
    printf("arg=%lu\n", arg);
    return NULL;
}

void posix_test(void)
{
    void posix_init_libc(void);

    posix_init_libc();

    pthread_t t;

    if (pthread_create(&t, NULL, thread, (void*)12345) != 0)
    {
        fprintf(stderr, "pthread_create() failed\n");
        abort();
    }

    if (pthread_join(t, NULL) != 0)
    {
        fprintf(stderr, "pthread_join() failed\n");
        abort();
    }

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
