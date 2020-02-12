#include <syscall.h>
#include "posix_io.h"
#include "posix_syscall.h"

ssize_t posix_read(int fd, void* buf, size_t count)
{
    return posix_syscall3(SYS_read, fd, (long)buf, (long)count);
}
