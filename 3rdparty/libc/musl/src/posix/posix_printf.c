#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>
#include "posix_io.h"

void posix_printf(const char* fmt, ...)
{
    char buf[4096];

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    posix_write(STDOUT_FILENO, buf, n);
}
