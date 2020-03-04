#include "posix_signal.h"

void posix_begin_ocall(void)
{
    posix_lock_signal();
}

void posix_end_ocall(void)
{
    posix_unlock_signal();
}
