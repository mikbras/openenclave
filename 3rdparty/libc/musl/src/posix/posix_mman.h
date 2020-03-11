#ifndef _POSIX_MMAN_H
#define _POSIX_MMAN_H

#include <stddef.h>
#include <sys/mman.h>

long posix_brk_syscall(void* new_brk);

long posix_mprotect_syscall(void* addr, size_t len, int prot);

long posix_mmap_syscall(
    void *addr,
    size_t length,
    int prot,
    int flags,
    int fd,
    off_t offset);

long posix_munmap_syscall(void *addr, size_t length);

#endif /* _POSIX_MMAN_H */
