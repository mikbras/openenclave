#include "pthread_impl.h"
#include "lock.h"

int pthread_kill(pthread_t t, int sig)
{
	int r;
	LOCK(&__UADDR(t->killlock[0]));
	r = t->tid ? -__syscall(SYS_tkill, t->tid, sig)
		: (sig+0U >= _NSIG ? EINVAL : 0);
	UNLOCK(&__UADDR(t->killlock[0]));
	return r;
}
