#include <unistd.h>
#include "test.h"
#define main memmem_oob_read_main
#include "../../../../../3rdparty/libc/libc-test/src/regression/memmem-oob-read.c"
