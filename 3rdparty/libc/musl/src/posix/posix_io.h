#ifndef _POSIX_IO_H
#define _POSIX_IO_H

#include <sys/uio.h>

__attribute__((format(printf, 1, 2)))
int posix_printf(const char* fmt, ...);

int posix_puts(const char* str);

long posix_write_syscall(int fd, const void* buf, size_t count);

long posix_writev_syscall(int fd, const struct iovec *iov, int iovcnt);

long posix_ioctl_syscall(
    int fd,
    unsigned long request,
    long arg1,
    long arg2,
    long arg3,
    long arg4);

#endif /* _POSIX_IO_H */
