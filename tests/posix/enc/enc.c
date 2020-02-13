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
    printf("=== thread()\n");
    return arg;
}

void posix_test(void)
{
    void posix_init_libc(void);
    posix_init_libc();

    void* p0 = malloc(4096);
    printf("p0=%p\n", p0);
    void* p1 = malloc(4096);
    printf("p1=%p\n", p1);
    void* p2 = malloc(4096);
    printf("p2=%p\n", p2);

    free(p0);
    free(p1);
    free(p2);

    for (size_t i = 0; i < 1000; i++)
        free(malloc(10));

#if 1
    pthread_t t;
    if (pthread_create(&t, NULL, thread, (void*)12345) != 0)
    {
        fprintf(stderr, "pthread_create() failed\n");
        abort();
    }
#endif

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
