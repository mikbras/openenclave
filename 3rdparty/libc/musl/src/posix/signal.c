#include <errno.h>
#include "posix_signal.h"
#include "posix_ocalls.h"
/* */
#include "posix_warnings.h"

int posix_rt_sigaction(
    int signum,
    const struct posix_sigaction* act,
    struct posix_sigaction* oldact,
    size_t sigsetsize)
{
    int r;

    if (posix_rt_sigaction_ocall(&r, signum, act, oldact, sigsetsize) != OE_OK)
        return -EINVAL;

    return r;
}
