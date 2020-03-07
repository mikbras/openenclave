#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <openenclave/enclave.h>
#include <openenclave/internal/tests.h>
#include <sys/syscall.h>
#include <signal.h>
#include "posix_t.h"

static void* _thread_func(void *arg)
{
    (void)arg;

    for (;;)
    {
        printf("_thread_func(): loop\n");
        fflush(stdout);
    }

#if 0
    for (;;)
        ;
#endif

    return NULL;
}

int test_pthread_cancel3(void)
{
    pthread_t td;
    void *res;

    printf("=== %s()\n", __FUNCTION__);

    OE_TEST(pthread_create(&td, 0, _thread_func, NULL) == 0);
    OE_TEST(pthread_cancel(td) == 0);
    OE_TEST(pthread_join(td, &res) == 0);
    OE_TEST(res == PTHREAD_CANCELED);

    return 0;
}
