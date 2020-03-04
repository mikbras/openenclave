#include "pthread_impl.h"
#include "lock.h"

int pthread_kill(pthread_t t, int sig)
{
	int r;
	LOCK(&FUTEX_MAP(t->zzzkilllock[0]));
	r = t->tid ? -__syscall(SYS_tkill, t->tid, sig)
		: (sig+0U >= _NSIG ? EINVAL : 0);
	UNLOCK(&FUTEX_MAP(t->zzzkilllock[0]));
	return r;
}
