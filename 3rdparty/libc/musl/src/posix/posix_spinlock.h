#ifndef _POSIX_SPINLOCK_H
#define _POSIX_SPINLOCK_H

#include <stdint.h>

#define POSIX_SPINLOCK_INITIALIZER 0

typedef volatile uint32_t posix_spinlock_t;

static __inline unsigned int __posix_spin_set_locked(posix_spinlock_t* s)
{
    unsigned int value = 1;

    __asm__ volatile("lock xchg %0, %1;"
                 : "=r"(value)     /* %0 */
                 : "m"(*s), /* %1 */
                   "0"(value)      /* also %2 */
                 : "memory");

    return value;
}

static __inline void posix_spin_lock(posix_spinlock_t* s)
{
    if (s)
    {
        while (__posix_spin_set_locked((volatile unsigned int*)s) != 0)
        {
            while (*s)
                __asm__ volatile("pause");
        }
    }
}

static __inline void posix_spin_unlock(posix_spinlock_t* s)
{
    if (s)
    {
        __asm__ volatile("movl %0, %1;"
            :
            : "r"(0), "m"(*s) /* %1 */
            : "memory");
    }
}

#endif /* _POSIX_SPINLOCK_H */
