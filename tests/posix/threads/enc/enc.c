// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#include <openenclave/corelibc/string.h>
#include <openenclave/enclave.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <openenclave/internal/print.h>
#include <openenclave/internal/tests.h>
#include <pthread.h>
#include <unistd.h>
#include <openenclave/internal/backtrace.h>
#include "posix_t.h"
#include "../../../../3rdparty/libc/musl/src/posix/posix_ocalls.h"

void posix_init(int* host_uaddr);

extern bool oe_disable_debug_malloc_check;

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

void test_create_thread(void)
{
    pthread_t threads[16];
    const size_t NUM_THREADS = OE_COUNTOF(threads);

    printf("=== %s()\n", __FUNCTION__);

    /* Create threads */
    for (size_t i = 0; i < NUM_THREADS; i++)
    {
        if (pthread_create(&threads[i], NULL, _thread_func, (void*)i) != 0)
        {
            fprintf(stderr, "pthread_create() failed\n");
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

    printf("=== %s() passed\n", __FUNCTION__);
}

static uint64_t _shared_integer = 0;
static pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;
const size_t N = 100;

static void* _test_mutex_thread(void* arg)
{
    size_t n = (uint64_t)arg;

    for (size_t i = 0; i < n*N; i++)
    {
        pthread_mutex_lock(&_mutex);
        _shared_integer++;
        pthread_mutex_unlock(&_mutex);
    }

    return arg;
}

void test_mutexes(void)
{
    pthread_t threads[16];
    const size_t NUM_THREADS = OE_COUNTOF(threads);
    size_t integer = 0;

    printf("=== %s()\n", __FUNCTION__);

    /* Create threads */
    for (size_t i = 0; i < NUM_THREADS; i++)
    {
        void* arg = (void*)i;

        if (pthread_create(&threads[i], NULL, _test_mutex_thread, arg) != 0)
        {
            fprintf(stderr, "pthread_create() failed\n");
            abort();
        }

        integer += i * N;
    }

    /* Join threads */
    for (size_t i = 0; i < NUM_THREADS; i++)
    {
        void* retval;
        OE_TEST(pthread_join(threads[i], &retval) == 0);
        OE_TEST((uint64_t)retval == i);
        printf("joined...\n");
    }

    OE_TEST(integer == _shared_integer);

    printf("=== %s() passed\n", __FUNCTION__);
}

static pthread_mutex_t _timed_mutex = PTHREAD_MUTEX_INITIALIZER;

static __int128 _time(void)
{
    const __int128 BILLION = 1000000000;
    struct timespec now;

    clock_gettime(CLOCK_REALTIME, &now);

    return (__int128)now.tv_sec * BILLION + (__int128)now.tv_nsec;
}

static void* _test_timedlock(void* arg)
{
    (void)arg;
    const uint64_t TIMEOUT_SEC = 3;
    const __int128 BILLION = 1000000000;
    const __int128 LO = (TIMEOUT_SEC * BILLION) - (BILLION / 5);
    const __int128 HI = (TIMEOUT_SEC * BILLION) + (BILLION / 5);

    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += TIMEOUT_SEC;

    __int128 t1 = _time();

    int r = pthread_mutex_timedlock(&_timed_mutex, &timeout);
    OE_TEST(r == ETIMEDOUT);

    __int128 t2 = _time();
    __int128 delta = t2 - t1;
    OE_TEST(delta >= LO && delta <= HI);

    return NULL;
}

void test_timedlock(void)
{
    pthread_t thread;

    printf("=== %s()\n", __FUNCTION__);

    pthread_mutex_lock(&_timed_mutex);

    if (pthread_create(&thread, NULL, _test_timedlock, NULL) != 0)
    {
        fprintf(stderr, "pthread_create() failed\n");
        abort();
    }

    for (size_t i = 0; i < 6; i++)
    {
        printf("sleeping...\n");
        sleep(1);
    }

    if (pthread_join(thread, NULL) != 0)
    {
        fprintf(stderr, "pthread_create() failed\n");
        abort();
    }

    pthread_mutex_unlock(&_timed_mutex);

    printf("=== %s() passed\n", __FUNCTION__);
}

void posix_test_ecall(int* host_uaddr)
{
    oe_disable_debug_malloc_check = true;

    posix_init(host_uaddr);

    test_create_thread();

    test_mutexes();

    test_timedlock();
}

OE_SET_ENCLAVE_SGX(
    1,    /* ProductID */
    1,    /* SecurityVersion */
    true, /* AllowDebug */
    1024, /* HeapPageCount */
    1024, /* StackPageCount */
    17);   /* TCSCount */
