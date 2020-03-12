#include <unistd.h>
#include "test.h"
#define main wcsncpy_read_overflow_main
#include "../../../../../3rdparty/libc/libc-test/src/regression/wcsncpy-read-overflow.c"
