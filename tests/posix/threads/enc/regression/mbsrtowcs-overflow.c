#include <unistd.h>
#include "test.h"
#define main mbsrtowcs_overflow_main
#include "../../../../../3rdparty/libc/libc-test/src/regression/mbsrtowcs-overflow.c"
