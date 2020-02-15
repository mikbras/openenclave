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
#if 0
    for (size_t i = 0; i < 1000; i++)
    {
        printf("_thread_func: %zu\n", i);
    }

    //printf("_thread_func(): arg=%lu\n", (uint64_t)arg);
#endif
    //printf("thread{%p}\n", (void*)pthread_self());
    //printf("sleep\n");
    sleep(1);

#if 0
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 1000;
    nanosleep(&ts, NULL);
#endif
    //printf("after sleep\n");

    return arg;
}

static volatile int* _uaddrs;
static size_t _uaddrs_size;

void posix_init_ecall(int* uaddrs, size_t uaddrs_size)
{
    _uaddrs = (volatile int*)uaddrs;
    _uaddrs_size = uaddrs_size;
}

void posix_init(volatile int* uaddrs, size_t uaddrs_size);

extern bool oe_disable_debug_malloc_check;

void posix_test_ecall(void)
{
    oe_disable_debug_malloc_check = true;

    pthread_t threads[16];
    const size_t NUM_THREADS = OE_COUNTOF(threads);

    posix_init(_uaddrs, _uaddrs_size);

    /* Create threads */
    for (size_t i = 0; i < NUM_THREADS; i++)
    {
oe_host_printf("BEFORE\n");
        if (pthread_create(&threads[i], NULL, _thread_func, (void*)12345) != 0)
        {
            fprintf(stderr, "EEE: pthread_create() failed\n");
            abort();
        }
oe_host_printf("AFTER\n");
    }

    /* Join threads */
    for (size_t i = 0; i < NUM_THREADS; i++)
    {
        void* retval;

#if 0
        printf("T1.JOINING...\n");
#endif

        if (pthread_join(threads[i], &retval) != 0)
        {
            fprintf(stderr, "pthread_join() failed\n");
            abort();
        }

#if 0
        printf("T1.JOININED...\n");
#endif

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
    32);   /* TCSCount */
