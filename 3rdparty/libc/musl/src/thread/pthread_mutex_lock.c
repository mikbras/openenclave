#include "pthread_impl.h"

int __pthread_mutex_lock(pthread_mutex_t *m)
{
	if ((m->_m_type&15) == PTHREAD_MUTEX_NORMAL
	    && !a_cas(&FUTEX_MAP(m->zzz_m_lock), 0, EBUSY))
		return 0;

	return __pthread_mutex_timedlock(m, 0);
}

weak_alias(__pthread_mutex_lock, pthread_mutex_lock);
