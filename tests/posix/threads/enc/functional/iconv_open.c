#include <unistd.h>
#include "test.h"
#define main iconv_open_main
#include "../../../../../3rdparty/libc/libc-test/src/functional/iconv_open.c"
