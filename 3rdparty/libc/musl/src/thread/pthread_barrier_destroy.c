#include "pthread_impl.h"

int pthread_barrier_destroy(pthread_barrier_t *b)
{
	if (b->_b_limit < 0) {
		if (__UADDR(b->_b_lock)) {
			int v;
			a_or(&__UADDR(b->_b_lock), INT_MIN);
			while ((v = __UADDR(b->_b_lock)) & INT_MAX)
				__wait(&__UADDR(b->_b_lock), 0, v, 0);
		}
		__vm_wait();
	}
	return 0;
}
