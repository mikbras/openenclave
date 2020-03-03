#include "pthread_impl.h"

int pthread_cond_signal(pthread_cond_t *c)
{
	if (!c->_c_shared) return __private_cond_signal(c, 1);
	if (!FUTEX_MAP(c->zzz_c_waiters)) return 0;
	a_inc(&FUTEX_MAP(c->zzz_c_seq));
	__wake(&FUTEX_MAP(c->zzz_c_seq), 1, 0);
	return 0;
}
