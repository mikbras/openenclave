#include "posix_syscall.h"
#include "posix_io.h"
#include <syscall.h>

ssize_t posix_writev(int fd, const struct iovec *iov, int iovcnt)
{
    long x1 = fd;
    long x2 = (long)iov;
    long x3 = (long)iovcnt;

    return (ssize_t)posix_syscall3(SYS_writev, x1, x2, x3);
}
