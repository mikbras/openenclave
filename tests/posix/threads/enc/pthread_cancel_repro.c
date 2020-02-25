#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <unistd.h>
#include <openenclave/enclave.h>
#include <openenclave/internal/tests.h>

extern int posix_gettid();

static void cleanup1(void *arg)
{
	*(int *)arg = 1;
}

static void *start_single(void *arg)
{
	pthread_cleanup_push(cleanup1, arg);
	sleep(3);
	pthread_cleanup_pop(0);
	return 0;
}

int pthread_cancel_repro(void)
{
	pthread_t td;
	sem_t sem1;
	void *res;
	int foo[4];

	OE_TEST(sem_init(&sem1, 0, 0) == 0);

#if 0
	/* Asynchronous cancellation */
	TESTR(r, pthread_create(&td, 0, start_async, &sem1), "failed to create thread");
	while (sem_wait(&sem1));
	TESTR(r, pthread_cancel(td), "canceling");
	TESTR(r, pthread_join(td, &res), "joining canceled thread");
	TESTC(res == PTHREAD_CANCELED, "canceled thread exit status");
#endif

	/* Cancellation cleanup handlers */
	foo[0] = 0;
	OE_TEST(pthread_create(&td, 0, start_single, foo) == 0);
	OE_TEST(pthread_cancel(td) == 0);
	OE_TEST(pthread_join(td, &res) == 0);
	OE_TEST(res == PTHREAD_CANCELED);
	OE_TEST(foo[0] == 1);

#if 0
	/* Nested cleanup handlers */
	memset(foo, 0, sizeof foo);
	TESTR(r, pthread_create(&td, 0, start_nested, foo), "failed to create thread");
	TESTR(r, pthread_cancel(td), "cancelling");
	TESTR(r, pthread_join(td, &res), "joining canceled thread");
	TESTC(res == PTHREAD_CANCELED, "canceled thread exit status");
	TESTC(foo[0] == 1, "cleanup handler failed to run");
	TESTC(foo[1] == 2, "cleanup handler failed to run");
	TESTC(foo[2] == 3, "cleanup handler failed to run");
	TESTC(foo[3] == 4, "cleanup handler failed to run");
#endif

	return 0;
}
