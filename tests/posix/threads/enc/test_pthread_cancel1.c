#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <openenclave/enclave.h>
#include <openenclave/internal/tests.h>

static void* _thread_func(void *arg)
{
    int* foo = (int*)arg;
    foo[0] = 12345;
    sleep(2);
    return 0;
}

int test_pthread_cancel1(void)
{
    pthread_t td;
    void *res;
    int foo[4];

    printf("=== test_pthread_cancel1()\n");
    fflush(stdout);

    foo[0] = 0;
    OE_TEST(pthread_create(&td, 0, _thread_func, foo) == 0);
    sleep(1);
    OE_TEST(pthread_cancel(td) == 0);
    OE_TEST(pthread_join(td, &res) == 0);
    OE_TEST(res == PTHREAD_CANCELED);
    OE_TEST(foo[0] == 12345);

    return 0;
}
