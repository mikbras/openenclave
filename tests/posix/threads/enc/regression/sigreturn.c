#include <unistd.h>
#include "test.h"
#define main sigreturn_main
#include "../../../../../3rdparty/libc/libc-test/src/regression/sigreturn.c"
