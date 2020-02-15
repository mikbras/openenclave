#include "pthread_impl.h"

static volatile int vmlock[2];

void __vm_wait()
{
	int tmp;
	while ((tmp=__UADDR(vmlock[0])))
		__wait(&__UADDR(vmlock[0]), &vmlock[1], tmp, 1);
}

void __vm_lock()
{
	a_inc(&__UADDR(vmlock[0]));
}

void __vm_unlock()
{
	if (a_fetch_add(&__UADDR(vmlock[0]), -1)==1 && vmlock[1])
		__wake(&__UADDR(vmlock[0]), -1, 1);
}
