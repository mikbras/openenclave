#include <unistd.h>
#include "test.h"
#define main setjmp_main
#include "../../../../../3rdparty/libc/libc-test/src/functional/setjmp.c"
