#include <semaphore.h>

int sem_getvalue(sem_t *restrict sem, int *restrict valp)
{
	int val = FUTEX_MAP(sem->zzz__val[0]);
	*valp = val < 0 ? 0 : val;
	return 0;
}
