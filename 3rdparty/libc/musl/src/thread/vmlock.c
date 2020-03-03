#include "pthread_impl.h"

static volatile int zzzvmlock[2];

void __vm_wait()
{
        int tmp;
        while ((tmp=FUTEX_MAP(zzzvmlock[0])))
                __wait(FUTEX_MAP_PTR(zzzvmlock), zzzvmlock+1, tmp, 1);
}

void __vm_lock()
{
        a_inc(FUTEX_MAP_PTR(zzzvmlock));
}

void __vm_unlock()
{
        if (a_fetch_add(FUTEX_MAP_PTR(zzzvmlock), -1)==1 && zzzvmlock[1])
                __wake(FUTEX_MAP_PTR(zzzvmlock), -1, 1);
}
