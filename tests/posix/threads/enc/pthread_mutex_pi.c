#include <unistd.h>
#include <openenclave/internal/print.h>
#define main pthread_mutex_pi_main
#pragma GCC diagnostic ignored "-Wunused"

extern int posix_gettid();

#include "../../../../3rdparty/libc/libc-test/src/functional/pthread_mutex_pi.c"
