#include <unistd.h>
#include "test.h"
#define main pthread_robust_main
#include "../../../../../3rdparty/libc/libc-test/src/functional/pthread_robust.c"
