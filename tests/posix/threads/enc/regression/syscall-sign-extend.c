#include <unistd.h>
#include "test.h"
#define main syscall_sign_extend_main
#include "../../../../../3rdparty/libc/libc-test/src/regression/syscall-sign-extend.c"
