#include <unistd.h>
#include "test.h"
#define main tls_align_dso_main
#include "../../../../../3rdparty/libc/libc-test/src/functional/tls_align_dso.c"
