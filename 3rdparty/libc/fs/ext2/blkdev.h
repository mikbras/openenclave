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

ssize_t blkdev_read(
    blkdev_t* dev,
    size_t offset,
    void* data,
    size_t size);

ssize_t blkdev_write(
    blkdev_t* dev,
    size_t offset,
    const void* data,
    size_t size);

#endif /* _BLKDEV_H */
