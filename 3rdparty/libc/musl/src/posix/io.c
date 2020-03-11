#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <openenclave/internal/print.h>
#include <openenclave/corelibc/stdio.h>
#include "posix_common.h"
#include "posix_syscall.h"
#include "posix_io.h"
#include "posix_signal.h"
#include "posix_ocalls.h"
#include "posix_panic.h"
#include "posix_warnings.h"

int posix_puts(const char* str)
{
    ssize_t retval;
    size_t len = strlen(str);

    if (POSIX_OCALL(posix_write_ocall(
        &retval, STDOUT_FILENO, str, len), 0x3561a98c) != OE_OK)
    {
        POSIX_PANIC;
    }

    if (retval < 0 || (size_t)retval != len)
        return -1;

    return 0;
}

int posix_printf(const char* fmt, ...)
{
    /* ATTN: use dynamic memory */
    char buf[4096];

    va_list ap;
    va_start(ap, fmt);
    int n = oe_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (n > 0)
    {
        ssize_t retval;

        if (POSIX_OCALL(posix_write_ocall(
            &retval, STDOUT_FILENO, buf, (size_t)n), 0x0134b0f4) != OE_OK)
        {
            POSIX_PANIC;
        }

        return (int)retval;
    }

    return n;
}

long posix_write_syscall(int fd, const void* buf, size_t count)
{
    if (fd == STDOUT_FILENO || fd == STDERR_FILENO)
    {
        ssize_t retval;

        if (POSIX_OCALL(posix_write_ocall(
            &retval, fd, buf, count), 0xa23cc325) != OE_OK)
        {
            POSIX_PANIC;
        }

        return (ssize_t)retval;
    }
    else
    {
        POSIX_PANIC_MSG("posix_writev_syscall(): unsupported fd");
    }

    POSIX_PANIC;
    return -EBADFD;
}

long posix_writev_syscall(int fd, const struct iovec *iov, int iovcnt)
{
    if (fd == STDOUT_FILENO || fd == STDERR_FILENO)
    {
        size_t count = 0;

        for (int i = 0; i < iovcnt; i++)
        {
            ssize_t retval;
            const char* p = iov[i].iov_base;
            size_t n = iov[i].iov_len;

            if (POSIX_OCALL(posix_write_ocall(
                &retval, fd, p, n), 0x93181b2b) != OE_OK)
            {
                POSIX_PANIC;
                return -1;
            }

            if (retval != (ssize_t)n)
            {
                POSIX_PANIC;
                break;
            }

            count += (size_t)retval;
        }

        return (ssize_t)count;
    }
    else
    {
        POSIX_PANIC_MSG("posix_writev_syscall(): unsupported fd");
    }

    return -EBADFD;
}

#define TIOCGWINSZ 0x5413

static int _ioctl_tiocgwinsz(int fd, unsigned long request, long arg)
{
    (void)fd;
    (void)request;

    struct winsize
    {
        unsigned short int ws_row;
        unsigned short int ws_col;
        unsigned short int ws_xpixel;
        unsigned short int ws_ypixel;
    };
    struct winsize* p;

    if (!(p = (struct winsize*)arg))
        return -EINVAL;

    p->ws_row = 24;
    p->ws_col = 80;
    p->ws_xpixel = 0;
    p->ws_ypixel = 0;

    return 0;
}

long posix_ioctl_syscall(
    int fd,
    unsigned long request,
    long arg1,
    long arg2,
    long arg3,
    long arg4)
{
    (void)arg2;
    (void)arg3;
    (void)arg4;

    if (fd == STDOUT_FILENO && request == TIOCGWINSZ)
    {
        return _ioctl_tiocgwinsz(fd, request, arg1);
    }
    else
    {
        POSIX_PANIC_MSG("posix_ioctl(): unsupported operation");
    }

    return -EBADFD;
}
