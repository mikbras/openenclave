#include "posix_signal.h"

void posix_begin_ocall(uint32_t lock_id)
{
    posix_lock_signal(lock_id);
}

void posix_end_ocall(uint32_t lock_id)
{
    posix_unlock_signal(lock_id);
}
