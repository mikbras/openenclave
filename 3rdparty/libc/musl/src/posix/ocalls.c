#include "posix_signal.h"
#include "posix_thread.h"
#include "posix_io.h"
#include "posix_panic.h"

void posix_begin_ocall(uint32_t id)
{
    posix_spin_lock(&posix_shared_block()->ocall_lock);

    if (posix_shared_block()->zone != POSIX_ZONE_SYSCALL)
        posix_raw_puts("posix_begin_ocall(): failed precondition");

    posix_shared_block()->zone = POSIX_ZONE_OCALL;
}

void posix_end_ocall(uint32_t id)
{
    if (posix_shared_block()->zone != POSIX_ZONE_OCALL)
        posix_raw_puts("posix_end_ocall(): failed precondition");

    posix_shared_block()->zone = POSIX_ZONE_SYSCALL;
    posix_spin_unlock(&posix_shared_block()->ocall_lock);
}
