#include <unistd.h>
#include "test.h"
#define main setenv_oom_main
#include "../../../../../3rdparty/libc/libc-test/src/regression/setenv-oom.c"
