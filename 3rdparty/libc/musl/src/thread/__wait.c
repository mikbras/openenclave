#include "pthread_impl.h"

static void _wait(
    volatile int *addr, volatile int *waiters, int val, int priv, int syscall)
{
	int spins=100;
	if (priv) priv = FUTEX_PRIVATE;
	while (spins-- && (!waiters || !*waiters)) {
		if (*addr==val) a_spin();
		else return;
	}
	if (waiters) a_inc(waiters);
	while (*addr==val) {
		__syscall(syscall, addr, FUTEX_WAIT|priv, val, 0) != -ENOSYS
		|| __syscall(syscall, addr, FUTEX_WAIT, val, 0);
	}
	if (waiters) a_dec(waiters);
}

void __wait(volatile int *addr, volatile int *waiters, int val, int priv)
{
    return _wait(addr, waiters, val, priv, SYS_futex);
}

void __cwait(volatile int *addr, volatile int *waiters, int val, int priv)
{
    return _wait(addr, waiters, val, priv, SYS_cutex);
}
