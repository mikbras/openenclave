#include "posix_syscall.h"
#include "posix_io.h"
#include <syscall.h>

ssize_t posix_write(int fd, const void* buf, size_t count)
{
    return posix_syscall3(SYS_write, fd, (long)buf, (long)count);
}
