#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <openenclave/enclave.h>
#include <openenclave/internal/tests.h>

extern int posix_gettid();

#if 0
static void cleanup1(void *arg)
{
	*(int *)arg = 1;
}
#endif

static void *start_single(void *arg)
{
    (void)arg;
    sleep(3);
    return 0;
}

int pthread_cancel_repro(void)
{
	pthread_t td;
	sem_t sem1;
	void *res;
	int foo[4];

        printf("=== pthread_cancel_repro()\n");
        fflush(stdout);

	OE_TEST(sem_init(&sem1, 0, 0) == 0);

	foo[0] = 0;
	OE_TEST(pthread_create(&td, 0, start_single, foo) == 0);
sleep(1);
	OE_TEST(pthread_cancel(td) == 0);
	OE_TEST(pthread_join(td, &res) == 0);
	OE_TEST(res == PTHREAD_CANCELED);
//	OE_TEST(foo[0] == 1);

	return 0;
}
