#include "blkdev.h"

ssize_t blkdev_read(
    blkdev_t* dev,
    size_t offset,
    void* data,
    size_t size)
{
    uint32_t blkno;
    uint32_t i;
    uint32_t rem;
    uint8_t* ptr;

    if (!dev || !data)
        return -1;

    blkno = offset / BLKDEV_BLKSIZE;

    for (i = blkno, rem = size, ptr = (uint8_t*)data; rem; i++)
    {
        uint8_t blk[BLKDEV_BLKSIZE];
        uint32_t off; /* offset into this block */
        uint32_t len; /* bytes to read from this block */

        if (dev->get(dev, i, blk) != 0)
            return -1;

        /* If first block */
        if (i == blkno)
            off = offset % BLKDEV_BLKSIZE;
        else
            off = 0;

        len = BLKDEV_BLKSIZE - off;

        if (len > rem)
            len = rem;

        memcpy(ptr, &blk[off], len);
        rem -= len;
        ptr += len;
    }

    return size;
}

ssize_t blkdev_write(
    blkdev_t* dev,
    size_t offset,
    const void* data,
    size_t size)
{
    uint32_t blkno;
    uint32_t i;
    uint32_t rem;
    uint8_t* ptr;

    if (!dev || !data)
        return -1;

    blkno = offset / BLKDEV_BLKSIZE;

    for (i = blkno, rem = size, ptr = (uint8_t*)data; rem; i++)
    {
        uint8_t blk[BLKDEV_BLKSIZE];
        uint32_t off; /* offset into this block */
        uint32_t len; /* bytes to write from this block */

        /* Fetch the block */
        if (dev->get(dev, i, blk) != 0)
            return -1;

        /* If first block */
        if (i == blkno)
            off = offset % BLKDEV_BLKSIZE;
        else
            off = 0;

        len = BLKDEV_BLKSIZE - off;

        if (len > rem)
            len = rem;

        memcpy(&blk[off], ptr, len);
        rem -= len;
        ptr += len;

        /* Rewrite the block */
        if (dev->put(dev, i, blk) != 0)
            return -1;
    }

    return size;
}
