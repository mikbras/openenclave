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

#define POSIX_STRUCT(PREFIX,NAME) OE_CONCAT(t_,NAME)
#include "../../../../3rdparty/libc/musl/src/posix/posix_ocall_structs.h"
#include "../../../../3rdparty/libc/musl/src/posix/posix_structs.h"

extern int posix_init(
    oe_enclave_t* enclave,
    posix_shared_block_t** shared_block);

extern int posix_gettid();

int main(int argc, const char* argv[])
{
    oe_result_t result;
    const uint32_t flags = OE_ENCLAVE_FLAG_DEBUG;
    const oe_enclave_type_t type = OE_ENCLAVE_TYPE_SGX;
    oe_enclave_t* enclave;
    int tid = posix_gettid();
    posix_shared_block_t* shared_block;

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s ENCLAVE_PATH\n", argv[0]);
        return 1;
    }

    printf("MAIN.PID=%d\n", getpid());

#if 0
    oe_enclave_setting_context_switchless_t setting =
    {
        8,
        0
    };
    oe_enclave_setting_t settings[] = {
        {.setting_type = OE_ENCLAVE_SETTING_CONTEXT_SWITCHLESS,
         .u.context_switchless_setting = &setting}};

    result = oe_create_posix_enclave(
        argv[1], type, flags, settings, OE_COUNTOF(settings), &enclave);
#else
    result = oe_create_posix_enclave(argv[1], type, flags, NULL, 0, &enclave);
#endif
    OE_TEST(result == OE_OK);

    OE_TEST(posix_init(enclave, &shared_block) == 0);
    OE_TEST(shared_block != NULL);

    result = posix_test_ecall(enclave, shared_block, tid);
    OE_TEST(result == OE_OK);

    result = oe_terminate_enclave(enclave);
    OE_TEST(result == OE_OK);

    free(shared_block);

    printf("=== passed all tests (posix)\n");

    return 0;
}
