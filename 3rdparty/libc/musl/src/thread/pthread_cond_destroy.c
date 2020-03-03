#include "pthread_impl.h"

int pthread_cond_destroy(pthread_cond_t *c)
{
	if (c->_c_shared && FUTEX_MAP(c->zzz_c_waiters)) {
		int cnt;
		a_or(&FUTEX_MAP(c->zzz_c_waiters), 0x80000000);
		a_inc(&FUTEX_MAP(c->zzz_c_seq));
		__wake(&FUTEX_MAP(c->zzz_c_seq), -1, 0);
		while ((cnt = FUTEX_MAP(c->zzz_c_waiters)) & 0x7fffffff)
			__wait(&FUTEX_MAP(c->zzz_c_waiters), 0, cnt, 0);
	}
	return 0;
}
