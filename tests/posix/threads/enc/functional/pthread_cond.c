#include <unistd.h>
#include "test.h"
#define main pthread_cond_main
#include "../../../../../3rdparty/libc/libc-test/src/functional/pthread_cond.c"
