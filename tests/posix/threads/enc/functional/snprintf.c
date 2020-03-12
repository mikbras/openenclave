#include <unistd.h>
#include "test.h"
#define main snprintf_main
#include "../../../../../3rdparty/libc/libc-test/src/functional/snprintf.c"
