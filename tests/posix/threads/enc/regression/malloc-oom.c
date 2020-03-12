#include <unistd.h>
#include "test.h"
#define main malloc_oom_main
#include "../../../../../3rdparty/libc/libc-test/src/regression/malloc-oom.c"
