#include "pthread_impl.h"

int pthread_barrier_destroy(pthread_barrier_t *b)
{
	if (b->_b_limit < 0) {
		if (FUTEX_MAP(b->zzz_b_lock)) {
			int v;
			a_or(&FUTEX_MAP(b->zzz_b_lock), INT_MIN);
			while ((v = FUTEX_MAP(b->zzz_b_lock)) & INT_MAX)
				__wait(&FUTEX_MAP(b->zzz_b_lock), 0, v, 0);
		}
		__vm_wait();
	}
	return 0;
}
