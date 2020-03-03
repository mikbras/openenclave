#include <semaphore.h>
#include <limits.h>
#include <errno.h>

int sem_init(sem_t *sem, int pshared, unsigned value)
{
	if (value > SEM_VALUE_MAX) {
		errno = EINVAL;
		return -1;
	}
	FUTEX_MAP(sem->zzz__val[0]) = value;
	FUTEX_MAP(sem->zzz__val[1]) = 0;
	FUTEX_MAP(sem->zzz__val[2]) = pshared ? 0 : 128;
	return 0;
}
