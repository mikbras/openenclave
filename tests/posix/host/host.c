// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#include <limits.h>
#include <errno.h>
#include <openenclave/host.h>
#include <openenclave/internal/error.h>
#include <openenclave/internal/tests.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include "posix_u.h"

oe_enclave_t* enclave = NULL;

void* _thread_func(void* arg)
{
    uint64_t* cookie = (uint64_t*)arg;
    int retval;

    if (posix_run_thread_ecall(enclave, &retval, *cookie) != OE_OK)
    {
        fprintf(stderr, "posix_run_thread_ecall(): failed\n");
        exit(1);
    }

    if (retval != 0)
    {
        fprintf(stderr, "posix_run_thread_ecall(): retval=%d\n", retval);
        exit(1);
    }

    printf("_thread_func() exit...\n");
    return NULL;
}

int posix_start_thread_ocall(uint64_t cookie)
{
    pthread_t t;

    if (pthread_create(&t, NULL, _thread_func, (void*)&cookie) != 0)
        return -1;

    return 0;
}

int posix_gettid_ocall(void)
{
    return (int)syscall(__NR_gettid);
}

int posix_nanosleep_ocall(
    const struct posix_timespec* req,
    struct posix_timespec* rem)
{
    if (nanosleep((struct timespec*)req, (struct timespec*)rem) != 0)
        return -errno;

    return 0;
}

int posix_futex_wait_ocall(
    int* uaddr,
    int futex_op,
    int val,
    const struct posix_timespec* timeout)
{
    long r;

    r = syscall(__NR_futex, uaddr, futex_op, val, (struct timespec*)timeout);

    if (r != 0)
        return -errno;

    return 0;
}

int posix_futex_wake_ocall(
    int* uaddr,
    int futex_op,
    int val)
{
    long r;

    r = syscall(__NR_futex, uaddr, futex_op, val, NULL);

    if (r != 0)
        return -errno;

    return 0;
}

int _uaddrs[4096];

int main(int argc, const char* argv[])
{
    oe_result_t result;
    const uint32_t flags = oe_get_create_flags();
    const oe_enclave_type_t type = OE_ENCLAVE_TYPE_SGX;

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s ENCLAVE_PATH\n", argv[0]);
        return 1;
    }

    result = oe_create_posix_enclave(argv[1], type, flags, NULL, 0, &enclave);
    OE_TEST(result == OE_OK);

    result = posix_init_ecall(enclave, _uaddrs, OE_COUNTOF(_uaddrs));
    OE_TEST(result == OE_OK);

    result = posix_test_ecall(enclave);
    OE_TEST(result == OE_OK);

    result = oe_terminate_enclave(enclave);
    OE_TEST(result == OE_OK);

    printf("=== passed all tests (posix)\n");

    return 0;
}
