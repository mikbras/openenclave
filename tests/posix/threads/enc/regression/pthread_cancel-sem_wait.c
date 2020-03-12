#include <unistd.h>
#include "test.h"
#define main pthread_cancel_sem_wait_main
#include "../../../../../3rdparty/libc/libc-test/src/regression/pthread_cancel-sem_wait.c"
