#include <unistd.h>
#include "test.h"
#define main fdopen_main
#include "../../../../../3rdparty/libc/libc-test/src/functional/fdopen.c"
