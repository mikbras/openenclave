#include <unistd.h>
#include "test.h"
#define main pthread_create_oom_main
#include "../../../../../3rdparty/libc/libc-test/src/regression/pthread_create-oom.c"
