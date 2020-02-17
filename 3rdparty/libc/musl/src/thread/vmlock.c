#include "pthread_impl.h"

static volatile int vmlock[2];

void __vm_wait()
{
        int tmp;
        while ((tmp=vmlock[0]))
                __wait(vmlock, vmlock+1, tmp, 1);
}

void __vm_lock()
{
        a_inc(vmlock);
}

void __vm_unlock()
{
        __CUTEX_LOCK(vmlock);
        if (a_fetch_add(vmlock, -1)==1 && vmlock[1])
                __wake(vmlock, -1, 1);
        __CUTEX_UNLOCK(vmlock);
}
