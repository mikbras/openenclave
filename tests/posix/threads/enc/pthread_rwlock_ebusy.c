#include <unistd.h>
#include <openenclave/internal/print.h>
#define main pthread_rwlock_ebusy_main
#pragma GCC diagnostic ignored "-Wunused"

extern int posix_gettid();

#include "../../../../3rdparty/libc/libc-test/src/regression/pthread_rwlock-ebusy.c"
