#include "posix_signal.h"
#include "posix_thread.h"

void posix_begin_ocall(uint32_t id)
{
    posix_shared_block()->redzone++;
}

void posix_end_ocall(uint32_t id)
{
    posix_shared_block()->redzone--;
}
