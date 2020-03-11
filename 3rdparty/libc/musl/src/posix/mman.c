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

#define USE_DLMALLOC

static void* _memalign(size_t alignment, size_t size)
{
#if defined(USE_DLMALLOC)
    extern void* dlmemalign(size_t alignment, size_t size);
    return dlmemalign(alignment, size);
#else
    return oe_memalign(alignment, size);
#endif
}

static void _free(void* ptr)
{
#if defined(USE_DLMALLOC)
    extern void dlfree(void* ptr);
    dlfree(ptr);
#else
    oe_free(ptr);
#endif
}

void* posix_brk(void* new_brk)
{
    uint8_t* current;

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
    if (addr && len && (prot & (PROT_READ|PROT_WRITE)))
        return 0;

    POSIX_PANIC;

    return -EACCES;

}

void* posix_mmap(
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

        if (!(ptr = _memalign(4096, length)))
        {
            oe_assert("oe_memalign() failed" == NULL);
            return (void*)-1;
        }

        memset(ptr, 0, length);
        return ptr;
    }
    else
    {
        POSIX_PANIC_MSG("unsupported mmap options");
    }

    return (void*)-1;
}

int posix_munmap(void *addr, size_t length)
{
    POSIX_ASSUME(addr != NULL);

    if (addr && length)
    {
        memset(addr, 0, length);
        _free(addr);
        return 0;
    }

    return 0;
}
