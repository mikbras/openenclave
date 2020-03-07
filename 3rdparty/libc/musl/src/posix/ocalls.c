#include "posix_signal.h"
#include "posix_thread.h"
#include "posix_io.h"
#include "posix_assert.h"

void posix_begin_ocall(uint32_t id)
{
    posix_spin_lock(&posix_shared_block()->ocall_lock);
    POSIX_ASSERT(posix_shared_block()->zone == POSIX_ZONE_SYSCALL);
    posix_shared_block()->zone = POSIX_ZONE_OCALL;
}

void posix_end_ocall(uint32_t id)
{
    POSIX_ASSERT(posix_shared_block()->zone == POSIX_ZONE_OCALL);
    posix_shared_block()->zone = POSIX_ZONE_SYSCALL;
    posix_spin_unlock(&posix_shared_block()->ocall_lock);
}
