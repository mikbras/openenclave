#include <unistd.h>
#include "test.h"
#define main fcntl_main
#include "../../../../../3rdparty/libc/libc-test/src/functional/fcntl.c"
