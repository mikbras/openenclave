#include "posix_signal.h"
#include "posix_thread.h"
#include "posix_io.h"
#include "posix_panic.h"

void posix_begin_ocall(uint32_t id)
{
    posix_spin_lock(&posix_shared_block()->ocall_lock);

#if 1
    if (posix_shared_block()->redzone != 1)
        posix_raw_puts("posix_begin_ocall(): failed precondition");
#endif

    posix_shared_block()->redzone++;
}

void posix_end_ocall(uint32_t id)
{
#if 1
    if (posix_shared_block()->redzone != 2)
        posix_raw_puts("posix_end_ocall(): failed precondition");
#endif

    posix_shared_block()->redzone--;
    posix_spin_unlock(&posix_shared_block()->ocall_lock);
}
