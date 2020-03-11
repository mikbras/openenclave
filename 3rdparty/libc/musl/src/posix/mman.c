#include <stdint.h>
#include <errno.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <openenclave/internal/syscall/unistd.h>
#include <openenclave/corelibc/assert.h>
#include <openenclave/corelibc/stdlib.h>
#include "posix_common.h"
#include "posix_syscall.h"
#include "posix_io.h"
#include "posix_mman.h"
#include "posix_panic.h"
#include "posix_assume.h"

#include "posix_warnings.h"

extern void* dlmemalign(size_t alignment, size_t size);

extern void dlfree(void* ptr);

#if 0
#define MEMALIGN dlmemalign
#define FREE dlfree
#else
#define MEMALIGN oe_memalign
#define FREE oe_free
#endif

long posix_brk_syscall(void* new_brk)
{
    uint8_t* current;

    /* If argument is null, return current break value. */
    if (new_brk == NULL)
        return (long)oe_sbrk(0);

    /* Get the current break value. */
    if ((current = oe_sbrk(0)) == (void*)-1)
        return -1;

    intptr_t increment = (uint8_t*)new_brk - current;

    if (((uint8_t*)oe_sbrk(increment)) == (void*)-1)
        return -1;

    return (long)new_brk;
}

long posix_mprotect_syscall(void* addr, size_t len, int prot)
{
    if (addr && len && (prot & (PROT_READ|PROT_WRITE)))
        return 0;

    POSIX_PANIC;

    return -EACCES;

}

long posix_mmap_syscall(
    void *addr,
    size_t length,
    int prot,
    int flags,
    int fd,
    off_t offset)
{
    const int FLAGS = MAP_PRIVATE | MAP_ANON;

    (void)prot;

    if (!addr && fd == -1 && !offset && flags == FLAGS)
    {
        uint8_t* ptr;

        if (!(ptr = MEMALIGN(4096, length)))
        {
            POSIX_PANIC_MSG("memalign() failed");
            return -1;
        }

        memset(ptr, 0, length);
        return (long)ptr;
    }
    else
    {
        POSIX_PANIC_MSG("unsupported mmap options");
    }

    return -1;
}

long posix_munmap_syscall(void *addr, size_t length)
{
    POSIX_ASSUME(addr != NULL);

    if (addr && length)
    {
        FREE(addr);
        return 0;
    }

    return 0;
}
