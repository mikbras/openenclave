#include <unistd.h>
#include "test.h"
#define main execle_env_main
#include "../../../../../3rdparty/libc/libc-test/src/regression/execle-env.c"
