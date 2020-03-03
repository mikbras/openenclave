#include "stdio_impl.h"
#include "pthread_impl.h"

int __lockfile(FILE *f)
{
	int owner = FUTEX_MAP(f->zzzlock), tid = __pthread_self()->tid;
	if ((owner & ~MAYBE_WAITERS) == tid)
		return 0;
	owner = a_cas(&FUTEX_MAP(f->zzzlock), 0, tid);
	if (!owner) return 1;
	while ((owner = a_cas(&FUTEX_MAP(f->zzzlock), 0, tid|MAYBE_WAITERS))) {
		if ((owner & MAYBE_WAITERS) ||
		    a_cas(&FUTEX_MAP(f->zzzlock), owner, owner|MAYBE_WAITERS)==owner)
			__futexwait(&FUTEX_MAP(f->zzzlock), owner|MAYBE_WAITERS, 1);
	}
	return 1;
}

void __unlockfile(FILE *f)
{
	if (a_swap(&FUTEX_MAP(f->zzzlock), 0) & MAYBE_WAITERS)
		__wake(&FUTEX_MAP(f->zzzlock), 1, 1);
}
