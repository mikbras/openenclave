#include <semaphore.h>
#include "pthread_impl.h"

static void cleanup(void *p)
{
	a_dec(p);
}

int sem_timedwait(sem_t *restrict sem, const struct timespec *restrict at)
{
	pthread_testcancel();

	if (!sem_trywait(sem)) return 0;

	int spins = 100;
	while (spins-- && FUTEX_MAP(sem->zzz__val[0]) <= 0 && !FUTEX_MAP(sem->zzz__val[1])) a_spin();

	while (sem_trywait(sem)) {
		int r;
		a_inc(FUTEX_MAP_PTR(sem->zzz__val+1));
		a_cas(FUTEX_MAP_PTR(sem->zzz__val), 0, -1);
		pthread_cleanup_push(cleanup, (void *)(FUTEX_MAP_PTR(sem->zzz__val+1)));
		r = __timedwait_cp(FUTEX_MAP_PTR(sem->zzz__val), -1, CLOCK_REALTIME, at, FUTEX_MAP(sem->zzz__val[2]));
		pthread_cleanup_pop(1);
		if (r) {
			errno = r;
			return -1;
		}
	}
	return 0;
}
