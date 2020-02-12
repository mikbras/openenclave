#include "io.h"
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include "syscall.h"

void posix_printf(const char* fmt, ...)
{
    char buf[4096];

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    posix_write(STDOUT_FILENO, buf, n);
}

ssize_t posix_read(int fd, void* buf, size_t count)
{
    return posix_syscall3(SYS_read, fd, (long)buf, (long)count);
}

ssize_t posix_write(int fd, const void* buf, size_t count)
{
    return posix_syscall3(SYS_write, fd, (long)buf, (long)count);
}

ssize_t posix_writev(int fd, const struct iovec *iov, int iovcnt)
{
    long x1 = fd;
    long x2 = (long)iov;
    long x3 = (long)iovcnt;

    return (ssize_t)posix_syscall3(SYS_writev, x1, x2, x3);
}
