#include <unistd.h>
#include "test.h"
#define main pthread_robust_detach_main
#include "../../../../../3rdparty/libc/libc-test/src/regression/pthread-robust-detach.c"
