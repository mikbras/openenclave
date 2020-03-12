#include <errno.h>
#include <stdarg.h>
#include "pthread_impl.h"

int posix_clone(int (*fn)(void*), void* stack, int flags, void* arg, ...);

int __clone(int (*func)(void *), void *stack, int flags, void *arg, ...)
{
	va_list ap;

	va_start(ap, arg);
	int* ptid = va_arg(ap, pid_t*);
	void* tls = va_arg(ap, void*);
	int* ctid = va_arg(ap, pid_t*);
	va_end(ap);

        return posix_clone(func, stack, flags, arg, ptid, tls, ctid);
}
