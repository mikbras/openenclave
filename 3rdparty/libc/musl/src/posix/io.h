#ifndef _POSIX_IO_H
#define _POSIX_IO_H

#include <sys/uio.h>

ssize_t posix_read(int fd, void* buf, size_t count);

ssize_t posix_write(int fd, const void* buf, size_t count);

ssize_t posix_writev(int fd, const struct iovec *iov, int iovcnt);

__attribute__((format(printf, 1, 2)))
void posix_printf(const char* fmt, ...);

#endif /* _POSIX_IO_H */
