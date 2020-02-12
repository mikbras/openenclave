#include "posix_syscall.h"
#include "posix_mman.h"
#include <stdint.h>
#include <errno.h>
#include <sys/mman.h>

void* posix_brk(void* new_brk)
{
    uint8_t* old;
    uint8_t* new;

    if ((old = (uint8_t*)posix_syscall1(SYS_brk, 0)) == (void*)-1)
        return (void*)-1;

    if ((new = (uint8_t*)posix_syscall1(SYS_brk, (long)new_brk)) == (void*)-1)
        return (void*)-1;

    if (new > old)
    {
        if (posix_mprotect(old, new - old, PROT_READ | PROT_WRITE) != 0)
            return (void*)-ENOMEM;
    }

    return new;
}

int posix_mprotect(void* addr, size_t len, int prot)
{
    long x1 = (long)addr;
    long x2 = (long)len;
    long x3 = (long)prot;
    return posix_syscall3(SYS_mprotect, x1, x2, x3);
}
