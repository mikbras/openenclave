#include <unistd.h>
#include "test.h"
#define main pthread_exit_cancel_main
#include "../../../../../3rdparty/libc/libc-test/src/regression/pthread_exit-cancel.c"
