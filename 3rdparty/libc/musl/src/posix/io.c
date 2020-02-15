#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <openenclave/internal/print.h>
#include "posix_syscall.h"
#include "posix_io.h"

#include "posix_warnings.h"

int posix_puts(const char* str)
{
    if (oe_host_write(0, str, strlen(str)) != 0)
        return -1;

    if (oe_host_write(0, "\n", 1) != 0)
        return -1;

    return 0;
}

int posix_printf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = oe_host_vfprintf(0, fmt, ap);
    va_end(ap);

    return n;
}

#if 0
ssize_t posix_read(int fd, void* buf, size_t count)
{
    return posix_syscall3(SYS_read, fd, (long)buf, (long)count);
}
#endif

ssize_t posix_write(int fd, const void* buf, size_t count)
{
    if (fd == STDOUT_FILENO || fd == STDERR_FILENO)
    {
        if (oe_host_write(fd - 1, buf, count) != 0)
            return -1;

        return (ssize_t)count;
    }

    assert("posix_write" == NULL);
    return -EBADFD;
}

ssize_t posix_writev(int fd, const struct iovec *iov, int iovcnt)
{
    if (fd == STDOUT_FILENO || fd == STDERR_FILENO)
    {
        size_t count = 0;

        for (int i = 0; i < iovcnt; i++)
        {
            if (oe_host_write(fd - 1, iov[i].iov_base, iov[i].iov_len) != 0)
                return -1;

            count += iov[i].iov_len;
        }

        return (ssize_t)count;
    }

    assert("posix_writev" == NULL);
    return -EBADFD;
}
