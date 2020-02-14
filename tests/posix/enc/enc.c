// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#include <openenclave/corelibc/string.h>
#include <openenclave/enclave.h>
#include <stdio.h>
#include <errno.h>
#include <openenclave/internal/print.h>
#include <openenclave/internal/tests.h>
#include <pthread.h>
#include <unistd.h>
#include <openenclave/internal/backtrace.h>
#include "posix_t.h"

static void* _thread_func(void* arg)
{
    for (size_t i = 0; i < 1000; i++)
    {
        printf("_thread_func: %zu\n", i);
    }

    //printf("_thread_func(): arg=%lu\n", (uint64_t)arg);
    printf("T2.SLEEPING...\n");
    sleep(3);

    return arg;
}

static volatile int* _uaddrs;
static size_t _uaddrs_size;

void posix_init_ecall(int* uaddrs, size_t uaddrs_size)
{
    _uaddrs = (volatile int*)uaddrs;
    _uaddrs_size = uaddrs_size;
}

extern bool oe_disable_debug_malloc_check;

void posix_test_ecall(void)
{
    oe_disable_debug_malloc_check = true;

    pthread_t t;
    extern void posix_init(volatile int* uaddrs, size_t uaddrs_size);

    posix_init(_uaddrs, _uaddrs_size);

    /* Create a new thread */
    if (pthread_create(&t, NULL, _thread_func, (void*)12345) != 0)
    {
        fprintf(stderr, "pthread_create() failed\n");
        abort();
    }

    /* Join with the other thread */
    {
        void* retval;

        printf("T1.JOINING...\n");

        if (pthread_join(t, &retval) != 0)
        {
            fprintf(stderr, "pthread_join() failed\n");
            abort();
        }

        printf("T1.JOININED...\n");

        OE_TEST((uint64_t)retval == 12345);
    }

    printf("=== posix_test()\n");
}

OE_SET_ENCLAVE_SGX(
    1,    /* ProductID */
    1,    /* SecurityVersion */
    true, /* AllowDebug */
    1024, /* HeapPageCount */
    1024, /* StackPageCount */
    2);   /* TCSCount */
