#include <unistd.h>
#include "test.h"
#define main sigaltstack_main
#include "../../../../../3rdparty/libc/libc-test/src/regression/sigaltstack.c"
