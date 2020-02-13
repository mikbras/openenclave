#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <openenclave/internal/print.h>
#include "posix_syscall.h"
#include "posix_io.h"

int posix_printf(const char* fmt, ...)
{
    char buf[4096];

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (oe_host_write(0, buf, n) != 0)
        return -1;

    return n;
}

ssize_t posix_read(int fd, void* buf, size_t count)
{
    return posix_syscall3(SYS_read, fd, (long)buf, (long)count);
}

ssize_t posix_write(int fd, const void* buf, size_t count)
{
    if (fd == STDOUT_FILENO || fd == STDERR_FILENO)
    {
        if (oe_host_write(fd - 1, buf, count) != 0)
            return -1;

        return count;
    }

    assert("posix_write" == NULL);
    return posix_syscall3(SYS_write, fd, (long)buf, (long)count);
}

ssize_t posix_writev(int fd, const struct iovec *iov, int iovcnt)
{
    long x1 = fd;
    long x2 = (long)iov;
    long x3 = (long)iovcnt;

    if (fd == STDOUT_FILENO || fd == STDERR_FILENO)
    {
        ssize_t count = 0;

        for (int i = 0; i < iovcnt; i++)
        {
            if (oe_host_write(fd - 1, iov[i].iov_base, iov[i].iov_len) != 0)
                return -1;

            count += iov[i].iov_len;
        }

        return count;
    }

    assert("posix_write" == NULL);
    return (ssize_t)posix_syscall3(SYS_writev, x1, x2, x3);
}
