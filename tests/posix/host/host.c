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
#include <unistd.h>
#include "posix_u.h"

oe_enclave_t* enclave = NULL;

void* _thread_func(void* arg)
{
    int* ptid = (int*)arg;
    int retval;

    if (posix_run_thread_ecall(enclave, &retval, *ptid) != OE_OK)
    {
        fprintf(stderr, "posix_run_thread_ecall(): failed\n");
        exit(1);
    }

    if (retval != 0)
    {
        fprintf(stderr, "posix_run_thread_ecall(): retval=%d\n", retval);
        exit(1);
    }

    return NULL;
}

int posix_start_thread_ocall(int tid)
{
    pthread_t t;

    if (pthread_create(&t, NULL, _thread_func, &tid) != 0)
        return -1;

    return 0;
}

int posix_futex_wait_ocall(
    int* uaddr,
    int futex_op,
    int val,
    struct posix_timespec* timeout)
{
    printf("posix_futex_wait_ocall(): *uaddr=%x val=%x\n", *uaddr, val);

    if (syscall(
        __NR_futex, uaddr, futex_op, val, (struct timespec*)timeout) != 0)
    {
        return -errno;
    }

    return 0;
}

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

    result = posix_test(enclave);
    OE_TEST(result == OE_OK);

    result = oe_terminate_enclave(enclave);
    OE_TEST(result == OE_OK);

    printf("=== passed all tests (posix)\n");

    return 0;
}
