#ifndef _BLKDEV_H
#define _BLKDEV_H

#include "config.h"

#define BLKDEV_BLKSIZE 512

typedef struct _Blkdev Blkdev;

struct _Blkdev
{
    int (*Close)(
        Blkdev* dev);

    int (*Get)(
        Blkdev* dev,
        UINTN blkno,
        void* data);

    int (*Put)(
        Blkdev* dev,
        UINTN blkno,
        const void* data);

    int (*SetFlags)(
        Blkdev* dev,
        UINT32 flags);
};

typedef enum _BlkdevAccess
{
    BLKDEV_ACCESS_RDWR,
    BLKDEV_ACCESS_RDONLY,
    BLKDEV_ACCESS_WRONLY
}
BlkdevAccess;

#endif /* _BLKDEV_H */
