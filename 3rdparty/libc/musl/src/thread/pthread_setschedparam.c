#include "pthread_impl.h"
#include "lock.h"

int pthread_setschedparam(pthread_t t, int policy, const struct sched_param *param)
{
	int r;
	LOCK(&FUTEX_MAP(t->zzzkilllock[0]));
	r = !t->tid ? ESRCH : -__syscall(SYS_sched_setscheduler, t->tid, policy, param);
	UNLOCK(&FUTEX_MAP(t->zzzkilllock[0]));
	return r;
}
