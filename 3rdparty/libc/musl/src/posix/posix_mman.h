#ifndef _POSIX_MMAN_H
#define _POSIX_MMAN_H

#include <stddef.h>
#include <sys/mman.h>

void* posix_brk(void* new_brk);

int posix_mprotect(void* addr, size_t len, int prot);

void* posix_mmap(
    void *addr,
    size_t length,
    int prot,
    int flags,
    int fd,
    off_t offset);

int posix_munmap(void *addr, size_t length);

#endif /* _POSIX_MMAN_H */
