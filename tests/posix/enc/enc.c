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

void sleep_msec(uint64_t milliseconds)
{
    struct timespec ts;
    const struct timespec* req = &ts;
    struct timespec rem = {0, 0};
    static const uint64_t _SEC_TO_MSEC = 1000UL;
    static const uint64_t _MSEC_TO_NSEC = 1000000UL;

    ts.tv_sec = (time_t)(milliseconds / _SEC_TO_MSEC);
    ts.tv_nsec = (long)((milliseconds % _SEC_TO_MSEC) * _MSEC_TO_NSEC);

    while (nanosleep(req, &rem) != 0 && errno == EINTR)
    {
        req = &rem;
    }
}

static void* _thread_func(void* arg)
{
    uint64_t secs = (size_t)arg;
    uint64_t msecs = secs * 1000;

    oe_host_printf("_thread_func()\n");
    sleep_msec(msecs / 10);

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
        if (pthread_create(&threads[i], NULL, _thread_func, (void*)i) != 0)
        {
            fprintf(stderr, "EEE: pthread_create() failed\n");
            abort();
        }
    }

    /* Join threads */
    for (size_t i = 0; i < NUM_THREADS; i++)
    {
        void* retval;

        if (pthread_join(threads[i], &retval) != 0)
        {
            fprintf(stderr, "pthread_join() failed\n");
            abort();
        }

        oe_host_printf("joined...\n");

        OE_TEST((uint64_t)retval == i);
    }

    printf("=== posix_test()\n");
}

OE_SET_ENCLAVE_SGX(
    1,    /* ProductID */
    1,    /* SecurityVersion */
    true, /* AllowDebug */
    1024, /* HeapPageCount */
    1024, /* StackPageCount */
    17);   /* TCSCount */
