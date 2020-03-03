#include <semaphore.h>
#include "pthread_impl.h"

int sem_post(sem_t *sem)
{
	int val, waiters, priv = FUTEX_MAP(sem->zzz__val[2]);
	do {
		val = FUTEX_MAP(sem->zzz__val[0]);
		waiters = FUTEX_MAP(sem->zzz__val[1]);
		if (val == SEM_VALUE_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
	} while (a_cas(FUTEX_MAP_PTR(sem->zzz__val), val, val+1+(val<0)) != val);
	if (val<0 || waiters) __wake(FUTEX_MAP_PTR(sem->zzz__val), 1, priv);
	return 0;
}
