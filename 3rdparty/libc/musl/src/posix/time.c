#include <errno.h>
#include <openenclave/enclave.h>
#include "posix_time.h"
#include "posix_io.h"

oe_result_t posix_nanosleep_ocall(
    int* retval,
    const struct posix_timespec* req,
    struct posix_timespec* rem);

int posix_nanosleep(const struct timespec* req, struct timespec* rem)
{
    int retval;

    if (posix_nanosleep_ocall(
        &retval,
        (const struct posix_timespec*)req,
        (struct posix_timespec*)rem) != OE_OK)
    {
        return -EINVAL;
    }

    return retval;
}
