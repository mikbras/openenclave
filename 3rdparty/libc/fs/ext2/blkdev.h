#ifndef _BLKDEV_H
#define _BLKDEV_H

#include "config.h"

#define BLKDEV_BLKSIZE 512

typedef struct _blkdev blkdev_t;

struct _blkdev
{
    int (*close)(blkdev_t* dev);

    int (*get)(blkdev_t* dev, uint64_t blkno, void* data);

    int (*put)(blkdev_t* dev, uint64_t blkno, const void* data);
};

#endif /* _BLKDEV_H */
