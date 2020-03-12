#include <unistd.h>
#include "test.h"
#define main dlopen_main
#include "../../../../../3rdparty/libc/libc-test/src/functional/dlopen.c"
