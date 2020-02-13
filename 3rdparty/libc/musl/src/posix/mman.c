#include <stdint.h>
#include <errno.h>
#include <sys/mman.h>
#include <openenclave/internal/syscall/unistd.h>
#include "posix_syscall.h"
#include "posix_io.h"
#include "posix_mman.h"

void* posix_brk(void* new_brk)
{
    uint8_t* current;
    uint8_t* new;

    /* If argument is null, return current break value. */
    if (new_brk == NULL)
        return oe_sbrk(0);

    /* Get the current break value. */
    if ((current = oe_sbrk(0)) == (void*)-1)
        return (void*)-1;

    intptr_t increment = (uint8_t*)new_brk - current;

    if (((uint8_t*)oe_sbrk(increment)) == (void*)-1)
        return (void*)-1;

    return new_brk;
}

int posix_mprotect(void* addr, size_t len, int prot)
{
    long x1 = (long)addr;
    long x2 = (long)len;
    long x3 = (long)prot;
    return posix_syscall3(SYS_mprotect, x1, x2, x3);
}
