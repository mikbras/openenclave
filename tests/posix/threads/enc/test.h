#ifndef _TEST_H
#define _TEST_H

#include <stdio.h>
#include <stdarg.h>

extern volatile int t_status;

#define t_error(FMT, ...) t_printf(__FILE__, __LINE__, FMT, __VA_ARGS__)

static __inline__ void t_printf(
    const char* file,
    unsigned line,
    const char* fmt,
    ...)
{
    va_list ap;

    fprintf(stderr, "%s(%u): ", file, line);

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    t_status = 1;
}

#endif /* _TEST_H */
