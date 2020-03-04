#include "pthread_impl.h"
#include "lock.h"

int pthread_setschedprio(pthread_t t, int prio)
{
	int r;
	LOCK(&FUTEX_MAP(t->zzzkilllock[0]));
	r = !t->tid ? ESRCH : -__syscall(SYS_sched_setparam, t->tid, &prio);
	UNLOCK(&FUTEX_MAP(t->zzzkilllock[0]));
	return r;
}
