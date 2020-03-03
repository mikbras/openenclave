#include <semaphore.h>
#include "pthread_impl.h"

int sem_trywait(sem_t *sem)
{
	int val;
	while ((val=FUTEX_MAP(sem->zzz__val[0])) > 0) {
		int new = val-1-(val==1 && FUTEX_MAP(sem->zzz__val[1]));
		if (a_cas(FUTEX_MAP_PTR(sem->zzz__val), val, new)==val) return 0;
	}
	errno = EAGAIN;
	return -1;
}
