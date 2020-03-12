#include <unistd.h>
#include "test.h"
#define main pthread_cancel_main
#include "../../../../../3rdparty/libc/libc-test/src/functional/pthread_cancel.c"
