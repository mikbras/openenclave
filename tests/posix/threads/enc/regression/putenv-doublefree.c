#include <unistd.h>
#include "test.h"
#define main putenv_doublefree_main
#include "../../../../../3rdparty/libc/libc-test/src/regression/putenv-doublefree.c"
