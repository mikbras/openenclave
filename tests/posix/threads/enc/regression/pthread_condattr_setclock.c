#include <unistd.h>
#include "test.h"
#define main pthread_condattr_setclock_main
#include "../../../../../3rdparty/libc/libc-test/src/regression/pthread_condattr_setclock.c"
