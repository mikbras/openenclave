#include <errno.h>
#include <openenclave/enclave.h>
#include <openenclave/internal/defs.h>
#include "posix_time.h"
#include "posix_io.h"

#include "posix_warnings.h"

oe_result_t posix_nanosleep_ocall(
    int* retval,
    const struct posix_timespec* req,
    struct posix_timespec* rem);

OE_STATIC_ASSERT(sizeof(struct timespec) == sizeof(struct posix_timespec));
OE_CHECK_FIELD(struct timespec, struct posix_timespec, tv_sec);
OE_CHECK_FIELD(struct timespec, struct posix_timespec, tv_nsec);

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
