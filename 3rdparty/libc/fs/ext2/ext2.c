/*
**==============================================================================
**
** LSVMTools
**
** MIT License
**
** Copyright (c) Microsoft Corporation. All rights reserved.
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
** SOFTWARE
**
**==============================================================================
*/
#include "config.h"
#include "ext2.h"
#include <time.h>
#include "buf.h"

#define EXT2_DECLARE_ERR(ERR) ext2_err_t ERR = EXT2_ERR_FAILED

/*
**==============================================================================
**
** tracing:
**
**==============================================================================
*/

#define TRACE printf("TRACE: %s(%u)\n", __FILE__, __LINE__)

#define GOTO_PRINT \
    printf("GOTO: %s(%d): %s()\n", __FILE__, __LINE__, __FUNCTION__);

#if defined(TRACE_GOTOS)
# define GOTO(LABEL) \
    do \
    { \
        GOTO_PRINT; \
        goto LABEL; \
    } \
    while (0)
#else
# define GOTO(LABEL) goto LABEL
#endif

/*
**==============================================================================
**
** local definitions:
**
**==============================================================================
*/

static size_t _strlcpy(char* dest, const char* src, size_t size)
{
    const char* start = src;

    if (size)
    {
        char* end = dest + size - 1;

        while (*src && dest != end)
            *dest++ = (char)*src++;

        *dest = '\0';
    }

    while (*src)
        src++;

    return (size_t)(src - start);
}

static char* _strncat(
    char* dest,
    uint64_t destSize,
    const char* src,
    uint64_t len)
{
    uint64_t n = strlen(dest);
    uint64_t i;

    for (i = 0; (i < len) && (n + i < destSize); i++)
    {
        dest[n + i] = src[i];
    }

    if (n + i < destSize)
        dest[n + i] = '\0';
    else if (destSize > 0)
        dest[destSize - 1] = '\0';

    return dest;
}

EXT2_INLINE uint32_t _next_mult(uint32_t x, uint32_t m)
{
    return (x + m - 1) / m * m;
}

EXT2_INLINE uint32_t _min(uint32_t x, uint32_t y)
{
    return x < y ? x : y;
}

static void _hex_dump(const uint8_t* data, uint32_t size, bool printables)
{
    uint32_t i;

    printf("%u bytes\n", size);

    for (i = 0; i < size; i++)
    {
        unsigned char c = data[i];

        if (printables && (c >= ' ' && c < '~'))
            printf("'%c", c);
        else
            printf("%02X", c);

        if ((i + 1) % 16)
        {
            printf(" ");
        }
        else
        {
            printf("\n");
        }
    }

    printf("\n");
}

static ssize_t _read(
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

static ssize_t _write(
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

static void _dump_block_numbers(const uint32_t* data, uint32_t size)
{
    uint32_t i;

    printf("%u blocks\n", size);

    for (i = 0; i < size; i++)
    {
        printf("%08X", data[i]);

        if ((i + 1) % 8)
        {
            printf(" ");
        }
        else
        {
            printf("\n");
        }
    }

    printf("\n");
}

static void _ascii_dump(const uint8_t* data, uint32_t size)
{
    uint32_t i;

    printf("=== ASCII dump:\n");

    for (i = 0; i < size; i++)
    {
        unsigned char c = data[i];

        if (c >= ' ' && c <= '~')
            printf("%c", c);
        else
            printf(".");

        if (i + 1 != size && !((i + 1) % 80))
            printf("\n");
    }

    printf("\n");
}

static bool _zero_filled(const uint8_t* data, uint32_t size)
{
    uint32_t i;

    for (i = 0; i < size; i++)
    {
        if (data[i])
            return 0;
    }

    return 1;
}

static uint32_t _count_bits(uint8_t byte)
{
    uint8_t i;
    uint8_t n = 0;

    for (i = 0; i < 8; i++)
    {
        if (byte & (1 << i))
            n++;
    }

    return n;
}

static uint32_t _count_bits_n(const uint8_t* data, uint32_t size)
{
    uint32_t i;
    uint32_t n = 0;

    for (i = 0; i < size; i++)
    {
        n += _count_bits(data[i]);
    }

    return n;
}

EXT2_INLINE bool _test_bit(const uint8_t* data, uint32_t size, uint32_t index)
{
    uint32_t byte = index / 8;
    uint32_t bit = index % 8;

    if (byte >= size)
        return 0;

    return ((uint32_t)(data[byte]) & (1 << bit)) ? 1 : 0;
}

EXT2_INLINE void _set_bit(uint8_t* data, uint32_t size, uint32_t index)
{
    uint32_t byte = index / 8;
    uint32_t bit = index % 8;

    if (byte >= size)
        return;

    data[byte] |= (1 << bit);
}

EXT2_INLINE void _clear_bit(uint8_t* data, uint32_t size, uint32_t index)
{
    uint32_t byte = index / 8;
    uint32_t bit = index % 8;

    if (byte >= size)
        return;

    data[byte] &= ~(1 << bit);
}

static void _dump_bitmap(const ext2_block_t* block)
{
    if (_zero_filled(block->data, block->size))
    {
        printf("...\n\n");
    }
    else
    {
        _hex_dump(block->data, block->size, 0);
    }
}

EXT2_INLINE bool _is_big_endian(void)
{
    union
    {
        unsigned short x;
        unsigned char bytes[2];
    }
    u;
    u.x = 0xABCD;
    return u.bytes[0] == 0xAB ? 1 : 0;
}

/*
**==============================================================================
**
** errors:
**
**==============================================================================
*/

const char* ext2_err_str(ext2_err_t err)
{
    if (err == EXT2_ERR_NONE)
        return "ok";

    if (err == EXT2_ERR_FAILED)
        return "failed";

    if (err == EXT2_ERR_INVALID_PARAMETER)
        return "invalid parameter";

    if (err == EXT2_ERR_FILE_NOT_FOUND)
        return "file not found";

    if (err == EXT2_ERR_BAD_MAGIC)
        return "bad magic";

    if (err == EXT2_ERR_UNSUPPORTED)
        return "unsupported";

    if (err == EXT2_ERR_OUT_OF_MEMORY)
        return "out of memory";

    if (err == EXT2_ERR_FAILED_TO_READ_SUPERBLOCK)
        return "failed to read superblock";

    if (err == EXT2_ERR_FAILED_TO_READ_GROUPS)
        return "failed to read groups";

    if (err == EXT2_ERR_FAILED_TO_READ_INODE)
        return "failed to read inode";

    if (err == EXT2_ERR_UNSUPPORTED_REVISION)
        return "unsupported revision";

    if (err == EXT2_ERR_OPEN_FAILED)
        return "open failed";

    if (err == EXT2_ERR_BUFFER_OVERFLOW)
        return "buffer overflow";

    if (err == EXT2_ERR_SEEK_FAILED)
        return "buffer overflow";

    if (err == EXT2_ERR_READ_FAILED)
        return "read failed";

    if (err == EXT2_ERR_WRITE_FAILED)
        return "write failed";

    if (err == EXT2_ERR_UNEXPECTED)
        return "unexpected";

    if (err == EXT2_ERR_SANITY_CHECK_FAILED)
        return "sanity check failed";

    if (err == EXT2_ERR_BAD_BLKNO)
        return "bad block number";

    if (err == EXT2_ERR_BAD_INO)
        return "bad inode number";

    if (err == EXT2_ERR_BAD_GRPNO)
        return "bad group number";

    if (err == EXT2_ERR_BAD_MULTIPLE)
        return "bad multiple";

    if (err == EXT2_ERR_EXTRANEOUS_DATA)
        return "extraneous data";

    if (err == EXT2_ERR_BAD_SIZE)
        return "bad size";

    if (err == EXT2_ERR_PATH_TOO_LONG)
        return "path too long";

    return "unknown";
}

/*
**==============================================================================
**
** blocks:
**
**==============================================================================
*/

/* Byte offset of this block (block 0 is the null block) */
static uint64_t block_offset(uint32_t blkno, uint32_t block_size)
{
    return blkno * block_size;
}

static uint32_t _make_blkno(const ext2_t* ext2, uint32_t grpno, uint32_t lblkno)
{
    const uint64_t first = ext2->sb.s_first_data_block;
    return (grpno * ext2->sb.s_blocks_per_group) + (lblkno + first);
}

static uint32_t _blkno_to_grpno(const ext2_t* ext2, uint32_t blkno)
{
    const uint64_t first = ext2->sb.s_first_data_block;

    if (first && blkno == 0)
        return 0;

    return (blkno - first) / ext2->sb.s_blocks_per_group;
}

static uint32_t _blkno_to_lblkno(const ext2_t* ext2, uint32_t blkno)
{
    const uint64_t first = ext2->sb.s_first_data_block;

    if (first && blkno == 0)
        return 0;

    return (blkno - first) % ext2->sb.s_blocks_per_group;
}

static ext2_err_t _read_blocks(
    const ext2_t* ext2,
    uint32_t blkno,
    uint32_t nblks,
    buf_t* buf)
{
    EXT2_DECLARE_ERR(err);
    uint32_t bytes;

    /* Check for null parameters */
    if (!ext2_valid(ext2) || !buf)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    /* Expand the allocation to hold the new data */
    {
        bytes = nblks * ext2->block_size;

        if (buf_reserve(buf, buf->size + bytes) != 0)
            GOTO(done);
    }

    /* Read the blocks */
    if (_read(
        ext2->dev,
        block_offset(blkno, ext2->block_size),
        (unsigned char*)buf->data + buf->size,
        bytes) != bytes)
    {
        err = EXT2_ERR_READ_FAILED;
        GOTO(done);
    }

    buf->size += bytes;

    err = EXT2_ERR_NONE;

done:

    return err;
}

ext2_err_t ext2_read_block(
    const ext2_t* ext2,
    uint32_t blkno,
    ext2_block_t* block)
{
    EXT2_DECLARE_ERR(err);

    /* Check for null parameters */
    if (!ext2_valid(ext2) || !block)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    memset(block, 0xAA, sizeof(ext2_block_t));

    /* Is block size too big for buffer? */
    if (ext2->block_size > sizeof(block->data))
    {
        err = EXT2_ERR_BUFFER_OVERFLOW;
        GOTO(done);
    }

    /* Set the size of the block */
    block->size = ext2->block_size;

    /* Read the block */
    if (_read(
        ext2->dev,
        block_offset(blkno, ext2->block_size),
        block->data,
        block->size) != block->size)
    {
        err = EXT2_ERR_READ_FAILED;
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:

    return err;
}

ext2_err_t ext2_write_block(
    const ext2_t* ext2,
    uint32_t blkno,
    const ext2_block_t* block)
{
    EXT2_DECLARE_ERR(err);

    /* Check for null parameters */
    if (!ext2_valid(ext2) || !block)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    /* Is block size too big for buffer? */
    if (block->size > ext2->block_size)
    {
        err = EXT2_ERR_BUFFER_OVERFLOW;
        GOTO(done);
    }

    /* Write the block */
    if (_write(
        ext2->dev,
        block_offset(blkno, ext2->block_size),
        block->data,
        block->size) != block->size)
    {
        err = EXT2_ERR_WRITE_FAILED;
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:

    return err;
}

static ext2_err_t _append_direct_block_numbers(
    const uint32_t* blocks,
    uint32_t num_blocks,
    buf_u32_t *buf)
{
    EXT2_DECLARE_ERR(err);
    uint32_t i;
    uint32_t n = 0;

    /* Determine size of blocks array */
    for (i = 0; i < num_blocks && blocks[i]; i++)
        n++;

    if (n == 0)
    {
        err = EXT2_ERR_NONE;
        goto done;
    }

    /* Append the blocks to the 'data' array */
    if ((err = buf_u32_append(buf, blocks, n)))
        GOTO(done);

    err = EXT2_ERR_NONE;

done:
    return err;
}

static ext2_err_t _append_indirect_block_numbers(
    const ext2_t* ext2,
    const uint32_t* blocks,
    uint32_t num_blocks,
    bool include_block_blocks,
    buf_u32_t *buf)
{
    EXT2_DECLARE_ERR(err);
    ext2_block_t block;
    uint32_t i;

    if (include_block_blocks)
    {
        if ((err = _append_direct_block_numbers(
            blocks,
            num_blocks,
            buf)))
        {
            GOTO(done);
        }
    }

    /* Handle the direct blocks */
    for (i = 0; i < num_blocks && blocks[i]; i++)
    {
        uint32_t block_no = blocks[i];

        /* Read the next block */
        if ((err = ext2_read_block(ext2, block_no, &block)))
        {
            GOTO(done);
        }

        if ((err = _append_direct_block_numbers(
            (const uint32_t*)block.data,
            block.size / sizeof(uint32_t),
            buf)))
        {
            GOTO(done);
        }
    }

    err = EXT2_ERR_NONE;

done:
    return err;
}

static ext2_err_t _append_double_indirect_block_numbers(
    const ext2_t* ext2,
    const uint32_t* blocks,
    uint32_t num_blocks,
    bool include_block_blocks,
    buf_u32_t *buf)
{
    EXT2_DECLARE_ERR(err);
    ext2_block_t block;
    uint32_t i;

    if (include_block_blocks)
    {
        if ((err = _append_direct_block_numbers(
            blocks,
            num_blocks,
            buf)))
        {
            GOTO(done);
        }
    }

    /* Handle the direct blocks */
    for (i = 0; i < num_blocks && blocks[i]; i++)
    {
        uint32_t block_no = blocks[i];

        /* Read the next block */
        if ((err = ext2_read_block(ext2, block_no, &block)))
        {
            GOTO(done);
        }

        if ((err = _append_indirect_block_numbers(
            ext2,
            (const uint32_t*)block.data,
            block.size / sizeof(uint32_t),
            include_block_blocks,
            buf)))
        {
            GOTO(done);
        }
    }

    err = EXT2_ERR_NONE;

done:
    return err;
}

static ext2_err_t _load_block_numbers_from_inode(
    const ext2_t* ext2,
    const ext2_inode_t* inode,
    bool include_block_blocks,
    buf_u32_t *buf)
{
    EXT2_DECLARE_ERR(err);

    /* Check parameters */
    if (!ext2_valid(ext2) || !inode || !buf)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    /* Handle the direct blocks */
    if ((err = _append_direct_block_numbers(
        inode->i_block,
        EXT2_SINGLE_INDIRECT_BLOCK,
        buf)))
    {
        GOTO(done);
    }

    /* Handle single-indirect blocks */
    if (inode->i_block[EXT2_SINGLE_INDIRECT_BLOCK])
    {
        ext2_block_t block;
        uint32_t block_no = inode->i_block[EXT2_SINGLE_INDIRECT_BLOCK];

        /* Read the next block */
        if ((err = ext2_read_block(ext2, block_no, &block)))
        {
            GOTO(done);
        }

        if (include_block_blocks)
        {
            if ((err = buf_u32_append(buf, &block_no, 1)))
            {
                GOTO(done);
            }
        }

        /* Append the block numbers from this block */
        if ((err = _append_direct_block_numbers(
            (const uint32_t*)block.data,
            block.size / sizeof(uint32_t),
            buf)))
        {
            GOTO(done);
        }
    }

    /* Handle double-indirect blocks */
    if (inode->i_block[EXT2_DOUBLE_INDIRECT_BLOCK])
    {
        ext2_block_t block;
        uint32_t block_no = inode->i_block[EXT2_DOUBLE_INDIRECT_BLOCK];

        /* Read the next block */
        if ((err = ext2_read_block(ext2, block_no, &block)))
        {
            GOTO(done);
        }

        if (include_block_blocks)
        {
            if ((err = buf_u32_append(buf, &block_no, 1)))
                GOTO(done);
        }

        if ((err = _append_indirect_block_numbers(
            ext2,
            (const uint32_t*)block.data,
            block.size / sizeof(uint32_t),
            include_block_blocks,
            buf)))
        {
            GOTO(done);
        }
    }

    /* Handle triple-indirect blocks */
    if (inode->i_block[EXT2_TRIPLE_INDIRECT_BLOCK])
    {
        ext2_block_t block;
        uint32_t block_no = inode->i_block[EXT2_TRIPLE_INDIRECT_BLOCK];

        /* Read the next block */
        if ((err = ext2_read_block(ext2, block_no, &block)))
        {
            GOTO(done);
        }

        if (include_block_blocks)
        {
            if ((err = buf_u32_append(buf, &block_no, 1)))
                GOTO(done);
        }

        if ((err = _append_double_indirect_block_numbers(
            ext2,
            (const uint32_t*)block.data,
            block.size / sizeof(uint32_t),
            include_block_blocks,
            buf)))
        {
            GOTO(done);
        }
    }

/* This is broken */
#if 0
    /* Check size expectations */
    if (!include_block_blocks)
    {
        uint32_t expected_size;

        expected_size = inode->i_size / ext2->block_size;

        if (inode->i_size % ext2->block_size)
            expected_size++;

        if (buf->size != expected_size)
        {
            err = EXT2_ERR_SANITY_CHECK_FAILED;
            GOTO(done);
        }
    }
#endif

    err = EXT2_ERR_NONE;

done:

    return err;
}

static ext2_err_t _write_group(const ext2_t* ext2, uint32_t grpno);

static ext2_err_t _write_super_block(const ext2_t* ext2);

static ext2_err_t _check_block_number(
    ext2_t* ext2,
    uint32_t blkno,
    uint32_t grpno,
    uint32_t lblkno)
{
    EXT2_DECLARE_ERR(err);

    /* Check parameters */
    if (!ext2)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    /* Sanity check */
    if (_make_blkno(ext2, grpno, lblkno) != blkno)
    {
        err = EXT2_ERR_BAD_BLKNO;
        GOTO(done);
    }

    /* See if 'grpno' is out of range */
    if (grpno > ext2->group_count)
    {
        err = EXT2_ERR_BAD_GRPNO;
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:
    return err;
}

static ext2_err_t _write_group_with_bitmap(
    ext2_t *ext2,
    uint32_t grpno,
    ext2_block_t* bitmap)
{
    EXT2_DECLARE_ERR(err);

    /* Check parameters */
    if (!ext2 || !bitmap)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    /* Write the group */
    if ((err = _write_group(ext2, grpno)))
    {
        GOTO(done);
    }

    /* Write the bitmap */
    if ((err = ext2_write_block_bitmap(ext2, grpno, bitmap)))
    {
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:
    return err;
}

static int _compare_uint32(const void* p1, const void* p2)
{
    uint32_t v1 = *(uint32_t*)p1;
    uint32_t v2 = *(uint32_t*)p2;

    if (v1 < v2)
        return -1;
    else if (v1 > v2)
        return 1;

    return 0;
}

static ext2_err_t _put_blocks(
    ext2_t* ext2,
    const uint32_t* blknos,
    uint32_t nblknos)
{
    EXT2_DECLARE_ERR(err);
    uint32_t i;
    uint32_t *temp = NULL;

    /* Sort the block numbers, so we can just iterate through the group list
       and make changes there. So we do I/O operations = to the group list
       rather than the number of blocks.
       NOTE: Hash table should be faster, but not a big deal for small files
       (initrd).
    */
    temp = (uint32_t*)malloc(nblknos * sizeof(uint32_t));
    if (temp == NULL)
    {
        err = EXT2_ERR_OUT_OF_MEMORY;
        GOTO(done);
    }
    for (i = 0; i < nblknos; i++)
    {
        temp[i] = blknos[i];
    }

    qsort(temp, nblknos, sizeof(uint32_t), _compare_uint32);

    /* Loop through the groups. */
    ext2_block_t bitmap;
    uint32_t prevgrpno = 0;
    for (i = 0; i < nblknos; i++)
    {
        uint32_t grpno = _blkno_to_grpno(ext2, temp[i]);
        uint32_t lblkno = _blkno_to_lblkno(ext2, temp[i]);

        if ((err = _check_block_number(ext2, temp[i], grpno, lblkno)))
        {
            GOTO(done);
        }

        if (i == 0)
        {
            if ((err = ext2_read_block_bitmap(ext2, grpno, &bitmap)))
            {
                GOTO(done);
            }
        }
        else if (prevgrpno != grpno)
        {
            /* Advanced to next group so write old bitmap and read new one. */
            if ((err = _write_group_with_bitmap(ext2, prevgrpno, &bitmap)))
            {
                GOTO(done);
            }
            if ((err = ext2_read_block_bitmap(ext2, grpno, &bitmap)))
            {
                GOTO(done);
            }
        }

        /* Sanity check */
        if (!_test_bit(bitmap.data, bitmap.size, lblkno))
        {
            err = EXT2_ERR_SANITY_CHECK_FAILED;
            GOTO(done);
        }

        /* Update in memory structs. */
        _clear_bit(bitmap.data, bitmap.size, lblkno);
        ext2->sb.s_free_blocks_count++;
        ext2->groups[grpno].bg_free_blocks_count++;
        prevgrpno = grpno;

        /* Always write final block. */
        if (i + 1 == nblknos)
        {
            if ((err = _write_group_with_bitmap(ext2, grpno, &bitmap)))
            {
                GOTO(done);
            }
        }
    }

    /* Update super block. */
    if ((err = _write_super_block(ext2)))
    {
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:
    free(temp);
    return err;
}

static ext2_err_t _get_block(ext2_t* ext2, uint32_t* blkno)
{
    EXT2_DECLARE_ERR(err);
    ext2_block_t bitmap;
    uint32_t grpno;

    /* Check parameters */
    if (!ext2 || !blkno)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    /* Clear any block number */
    *blkno = 0;

    /* Use brute force search for a free block */
    for (grpno = 0; grpno < ext2->group_count; grpno++)
    {
        uint32_t lblkno;

        /* Read the bitmap */

        if ((err = ext2_read_block_bitmap(ext2, grpno, &bitmap)))
        {
            GOTO(done);
        }

        /* Scan the bitmap, looking for free bit */
        for (lblkno = 0; lblkno < bitmap.size * 8; lblkno++)
        {
            if (!_test_bit(bitmap.data, bitmap.size, lblkno))
            {
                _set_bit(bitmap.data, bitmap.size, lblkno);
                *blkno = _make_blkno(ext2, grpno, lblkno);
                break;
            }
        }

        if (*blkno)
            break;
    }

    /* If no free blocks found */
    if (!*blkno)
    {
        GOTO(done);
    }

    /* Write the superblock */
    {
        ext2->sb.s_free_blocks_count--;

        if ((err = _write_super_block(ext2)))
        {
            GOTO(done);
        }
    }

    /* Write the group */
    {
        ext2->groups[grpno].bg_free_blocks_count--;

        if ((err = _write_group(ext2, grpno)))
        {
            GOTO(done);
        }
    }

    /* Write the bitmap */
    if ((err = ext2_write_block_bitmap(ext2, grpno, &bitmap)))
    {
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:
    return err;
}

/*
**==============================================================================
**
** super block:
**
**==============================================================================
*/

void ext2_dump_super_block(const ext2_super_block_t* sb)
{
    printf("=== ext2_super_block_t:\n");
    printf("s_inodes_count=%u\n", sb->s_inodes_count);
    printf("s_blocks_count=%u\n", sb->s_blocks_count);
    printf("s_r_blocks_count=%u\n", sb->s_r_blocks_count);
    printf("s_free_blocks_count=%u\n", sb->s_free_blocks_count);
    printf("s_free_inodes_count=%u\n", sb->s_free_inodes_count);
    printf("s_first_data_block=%u\n", sb->s_first_data_block);
    printf("s_log_block_size=%u\n", sb->s_log_block_size);
    printf("s_log_frag_size=%u\n", sb->s_log_frag_size);
    printf("s_blocks_per_group=%u\n", sb->s_blocks_per_group);
    printf("s_frags_per_group=%u\n", sb->s_frags_per_group);
    printf("s_inodes_per_group=%u\n", sb->s_inodes_per_group);
    printf("s_mtime=%u\n", sb->s_mtime);
    printf("s_wtime=%u\n", sb->s_wtime);
    printf("s_mnt_count=%u\n", sb->s_mnt_count);
    printf("s_max_mnt_count=%u\n", sb->s_max_mnt_count);
    printf("s_magic=%X\n", sb->s_magic);
    printf("s_state=%u\n", sb->s_state);
    printf("s_errors=%u\n", sb->s_errors);
    printf("s_minor_rev_level=%u\n", sb->s_minor_rev_level);
    printf("s_lastcheck=%u\n", sb->s_lastcheck);
    printf("s_checkinterval=%u\n", sb->s_checkinterval);
    printf("s_creator_os=%u\n", sb->s_creator_os);
    printf("s_rev_level=%u\n", sb->s_rev_level);
    printf("s_def_resuid=%u\n", sb->s_def_resuid);
    printf("s_def_resgid=%u\n", sb->s_def_resgid);
    printf("s_first_ino=%u\n", sb->s_first_ino);
    printf("s_inode_size=%u\n", sb->s_inode_size);
    printf("s_block_group_nr=%u\n", sb->s_block_group_nr);
    printf("s_feature_compat=%u\n", sb->s_feature_compat);
    printf("s_feature_incompat=%u\n", sb->s_feature_incompat);
    printf("s_feature_ro_compat=%u\n", sb->s_feature_ro_compat);
    printf("s_uuid="
        "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
        sb->s_uuid[0], sb->s_uuid[1], sb->s_uuid[2], sb->s_uuid[3],
        sb->s_uuid[4], sb->s_uuid[5], sb->s_uuid[6], sb->s_uuid[7],
        sb->s_uuid[8], sb->s_uuid[9], sb->s_uuid[10], sb->s_uuid[11],
        sb->s_uuid[12], sb->s_uuid[13], sb->s_uuid[14], sb->s_uuid[15]);
    printf("s_volume_name=%s\n", sb->s_volume_name);
    printf("s_last_mounted=%s\n", sb->s_last_mounted);
    printf("s_algo_bitmap=%u\n", sb->s_algo_bitmap);
    printf("s_prealloc_blocks=%u\n", sb->s_prealloc_blocks);
    printf("s_prealloc_dir_blocks=%u\n", sb->s_prealloc_dir_blocks);
    printf("s_journal_uuid="
        "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
        sb->s_journal_uuid[0], sb->s_journal_uuid[1], sb->s_journal_uuid[2],
        sb->s_journal_uuid[3], sb->s_journal_uuid[4], sb->s_journal_uuid[5],
        sb->s_journal_uuid[6], sb->s_journal_uuid[7], sb->s_journal_uuid[8],
        sb->s_journal_uuid[9], sb->s_journal_uuid[10], sb->s_journal_uuid[11],
        sb->s_journal_uuid[12], sb->s_journal_uuid[13], sb->s_journal_uuid[14],
        sb->s_journal_uuid[15]);
    printf("s_journal_inum=%u\n", sb->s_journal_inum);
    printf("s_journal_dev=%u\n", sb->s_journal_dev);
    printf("s_last_orphan=%u\n", sb->s_last_orphan);
    printf("s_hash_seed={%02X,%02X,%02X,%02X}\n",
        sb->s_hash_seed[0], sb->s_hash_seed[1],
        sb->s_hash_seed[2], sb->s_hash_seed[3]);
    printf("s_def_hash_version=%u\n", sb->s_def_hash_version);
    printf("s_default_mount_options=%u\n", sb->s_default_mount_options);
    printf("s_first_meta_bg=%u\n", sb->s_first_meta_bg);
    printf("\n");
}

static ext2_err_t _read_super_block(
    blkdev_t* dev,
    ext2_super_block_t* sb)
{
    EXT2_DECLARE_ERR(err);

    /* Read the superblock */
    if (_read(
        dev,
        EXT2_BASE_OFFSET,
        sb,
        sizeof(ext2_super_block_t)) != sizeof(ext2_super_block_t))
    {
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:
    return err;
}

static ext2_err_t _write_super_block(const ext2_t* ext2)
{
    EXT2_DECLARE_ERR(err);

    /* Read the superblock */
    if (_write(
        ext2->dev,
        EXT2_BASE_OFFSET,
        &ext2->sb,
        sizeof(ext2_super_block_t)) != sizeof(ext2_super_block_t))
    {
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:
    return err;
}

/*
**==============================================================================
**
** groups:
**
**==============================================================================
*/

static void _dump_group_desc(const ext2_group_desc_t* gd)
{
    printf("=== ext2_group_desc_t\n");
    printf("bg_block_bitmap=%u\n", gd->bg_block_bitmap);
    printf("bg_inode_bitmap=%u\n", gd->bg_inode_bitmap);
    printf("bg_inode_table=%u\n", gd->bg_inode_table);
    printf("bg_free_blocks_count=%u\n", gd->bg_free_blocks_count);
    printf("bg_free_inodes_count=%u\n", gd->bg_free_inodes_count);
    printf("bg_used_dirs_count=%u\n", gd->bg_used_dirs_count);
    printf("\n");
}

static void _dump_group_descs(
    const ext2_group_desc_t* groups,
    uint32_t group_count)
{
    const ext2_group_desc_t* p = groups;
    const ext2_group_desc_t* end = groups + group_count;

    while (p != end)
    {
        _dump_group_desc(p);
        p++;
    }
}

static ext2_group_desc_t* _read_groups(const ext2_t* ext2)
{
    EXT2_DECLARE_ERR(err);
    ext2_group_desc_t* groups = NULL;
    uint32_t groups_size = 0;
    uint32_t blkno;

    /* Check the file system argument */
    if (!ext2_valid(ext2))
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    /* Allocate the groups list */
    {
        groups_size = ext2->group_count * sizeof(ext2_group_desc_t);

        if (!(groups = (ext2_group_desc_t*)malloc(groups_size)))
        {
            GOTO(done);
        }

        memset(groups, 0xAA, groups_size);
    }

    /* Determine the block where group table starts */
    if (ext2->block_size == 1024)
        blkno = 2;
    else
        blkno = 1;

    /* Read the block */
    if (_read(
        ext2->dev,
        block_offset(blkno, ext2->block_size),
        groups,
        groups_size) != groups_size)
    {
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:

    if ((err))
    {
        if (groups)
        {
            free(groups);
            groups = NULL;
        }
    }

    return groups;
}

static ext2_err_t _write_group(const ext2_t* ext2, uint32_t grpno)
{
    EXT2_DECLARE_ERR(err);
    uint32_t blkno;

    /* Check the file system argument */
    if (!ext2_valid(ext2))
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    if (ext2->block_size == 1024)
        blkno = 2;
    else
        blkno = 1;

    /* Read the block */
    if (_write(
        ext2->dev,
        block_offset(blkno,ext2->block_size) +
            (grpno * sizeof(ext2_group_desc_t)),
        &ext2->groups[grpno],
        sizeof(ext2_group_desc_t)) != sizeof(ext2_group_desc_t))
    {
        err = EXT2_ERR_WRITE_FAILED;
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:

    return err;
}

/*
**==============================================================================
**
** inodes:
**
**==============================================================================
*/

static uint32_t _make_ino(const ext2_t* ext2, uint32_t grpno, uint32_t lino)
{
    return (grpno * ext2->sb.s_inodes_per_group) + (lino + 1);
}

static uint32_t _ino_to_grpno(const ext2_t* ext2, ext2_ino_t ino)
{
    if (ino == 0)
        return 0;

    return (ino-1) / ext2->sb.s_inodes_per_group;
}

static uint32_t _ino_to_lino(const ext2_t* ext2, ext2_ino_t ino)
{
    if (ino == 0)
        return 0;

    return (ino-1) % ext2->sb.s_inodes_per_group;
}

void ext2_dump_inode(const ext2_t* ext2, const ext2_inode_t* inode)
{
    uint32_t i;
    uint32_t n;
    (void)_hex_dump;
    (void)_ascii_dump;

    printf("=== ext2_inode_t\n");
    printf("i_mode=%u (%X)\n", inode->i_mode, inode->i_mode);
    printf("i_uid=%u\n", inode->i_uid);
    printf("i_size=%u\n", inode->i_size);
    printf("i_atime=%u\n", inode->i_atime);
    printf("i_ctime=%u\n", inode->i_ctime);
    printf("i_mtime=%u\n", inode->i_mtime);
    printf("i_dtime=%u\n", inode->i_dtime);
    printf("i_gid=%u\n", inode->i_gid);
    printf("i_links_count=%u\n", inode->i_links_count);
    printf("i_blocks=%u\n", inode->i_blocks);
    printf("i_flags=%u\n", inode->i_flags);
    printf("i_osd1=%u\n", inode->i_osd1);

    {
        printf("i_block[]={");
        n = sizeof(inode->i_block) / sizeof(inode->i_block[0]);

        for (i = 0; i < n; i++)
        {
            printf("%X", inode->i_block[i]);

            if (i + 1 != n)
                printf(", ");
        }

        printf("}\n");
    }

    printf("i_generation=%u\n", inode->i_generation);
    printf("i_file_acl=%u\n", inode->i_file_acl);
    printf("i_dir_acl=%u\n", inode->i_dir_acl);
    printf("i_faddr=%u\n", inode->i_faddr);

    {
        printf("i_osd2[]={");
        n = sizeof(inode->i_osd2) / sizeof(inode->i_osd2[0]);

        for (i = 0; i < n; i++)
        {
            printf("%u", inode->i_osd2[i]);

            if (i + 1 != n)
                printf(", ");
        }

        printf("}\n");
    }

    printf("\n");

    if (inode->i_block[0])
    {
        ext2_block_t block;

        if (!(ext2_read_block(ext2, inode->i_block[0], &block)))
        {
            _ascii_dump(block.data, block.size);
        }
    }
}

static ext2_err_t _write_inode_bitmap(
    const ext2_t* ext2,
    uint32_t group_index,
    const ext2_block_t* block)
{
    EXT2_DECLARE_ERR(err);
    uint32_t bitmap_size_bytes;

    if (!ext2_valid(ext2) || !block)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    bitmap_size_bytes = ext2->sb.s_inodes_per_group / 8;

    if (block->size != bitmap_size_bytes)
    {
        err = EXT2_ERR_BAD_SIZE;
        GOTO(done);
    }

    if (group_index > ext2->group_count)
    {
        err = EXT2_ERR_BAD_GRPNO;
        GOTO(done);
    }

    if ((err = ext2_write_block(
        ext2,
        ext2->groups[group_index].bg_inode_bitmap,
        block)))
    {
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:
    return err;
}

static ext2_err_t _get_ino(ext2_t* ext2, ext2_ino_t* ino)
{
    EXT2_DECLARE_ERR(err);
    ext2_block_t bitmap;
    uint32_t grpno;

    /* Check parameters */
    if (!ext2 || !ino)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    /* Clear the node number */
    *ino = 0;

    /* Use brute force search for a free inode number */
    for (grpno = 0; grpno < ext2->group_count; grpno++)
    {
        uint32_t lino;

        /* Read the bitmap */
        if ((err = ext2_read_inode_bitmap(ext2, grpno, &bitmap)))
        {
            GOTO(done);
        }

        /* Scan the bitmap, looking for free bit */
        for (lino = 0; lino < bitmap.size * 8; lino++)
        {
            if (!_test_bit(bitmap.data, bitmap.size, lino))
            {
                _set_bit(bitmap.data, bitmap.size, lino);
                *ino = _make_ino(ext2, grpno, lino);
                break;
            }
        }

        if (*ino)
            break;
    }

    /* If no free inode numbers */
    if (!*ino)
    {
        GOTO(done);
    }

    /* Write the superblock */
    {
        ext2->sb.s_free_inodes_count--;

        if ((err = _write_super_block(ext2)))
        {
            GOTO(done);
        }
    }

    /* Write the group */
    {
        ext2->groups[grpno].bg_free_inodes_count--;

        if ((err = _write_group(ext2, grpno)))
        {
            GOTO(done);
        }
    }

    /* Write the bitmap */
    if ((err = _write_inode_bitmap(ext2, grpno, &bitmap)))
    {
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:
    return err;
}

ext2_err_t ext2_read_inode(
    const ext2_t* ext2,
    ext2_ino_t ino,
    ext2_inode_t* inode)
{
    EXT2_DECLARE_ERR(err);
    uint32_t lino = _ino_to_lino(ext2, ino);
    uint32_t grpno = _ino_to_grpno(ext2, ino);
    const ext2_group_desc_t* group = &ext2->groups[grpno];
    uint32_t inode_size = ext2->sb.s_inode_size;
    uint64_t offset;

    if (ino == 0)
    {
        err = EXT2_ERR_BAD_INO;
        GOTO(done);
    }

    /* Check the reverse mapping */
    {
        ext2_ino_t tmp;
        tmp = _make_ino(ext2, grpno, lino);
        assert(tmp == ino);
    }

    offset = block_offset(group->bg_inode_table, ext2->block_size) +
        lino * inode_size;

    /* Read the inode */
    if (_read(
        ext2->dev,
        offset,
        inode,
        inode_size) != inode_size)
    {
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:
    return err;
}

static ext2_err_t _write_inode(
    const ext2_t* ext2,
    ext2_ino_t ino,
    const ext2_inode_t* inode)
{
    EXT2_DECLARE_ERR(err);
    uint32_t lino = _ino_to_lino(ext2, ino);
    uint32_t grpno = _ino_to_grpno(ext2, ino);
    const ext2_group_desc_t* group = &ext2->groups[grpno];
    uint32_t inode_size = ext2->sb.s_inode_size;
    uint64_t offset;

    /* Check the reverse mapping */
    {
        ext2_ino_t tmp;
        tmp = _make_ino(ext2, grpno, lino);
        assert(tmp == ino);
    }

    offset = block_offset(group->bg_inode_table, ext2->block_size) +
        lino * inode_size;

    /* Read the inode */
    if (_write(
        ext2->dev,
        offset,
        inode,
        inode_size) != inode_size)
    {
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:
    return err;
}

ext2_err_t ext2_path_to_ino(
    const ext2_t* ext2,
    const char* path,
    ext2_ino_t* ino)
{
    EXT2_DECLARE_ERR(err);
    char buf[EXT2_PATH_MAX];
    const char* elements[32];
    const uint8_t NELEMENTS = sizeof(elements) / sizeof(elements[0]);
    uint8_t nelements = 0;
    char* p;
    char* save;
    uint8_t i;
    ext2_ino_t current_ino = 0;

    /* Check for null parameters */
    if (!ext2 || !path || !ino)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    /* Initialize to null inode for now */
    *ino = 0;

    /* Check path length */
    if (strlen(path) >= EXT2_PATH_MAX)
    {
        err = EXT2_ERR_PATH_TOO_LONG;
        GOTO(done);
    }

    /* Copy path */
    _strlcpy(buf, path, sizeof(buf));

    /* Be sure path begins with "/" */
    if (path[0] != '/')
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    elements[nelements++] = "/";

    /* Split the path into components */
    for (p = strtok_r(buf, "/", &save); p; p = strtok_r(NULL, "/", &save))
    {
        if (nelements == NELEMENTS)
        {
            err = EXT2_ERR_BUFFER_OVERFLOW;
            GOTO(done);
        }

        elements[nelements++] = p;
    }

    /* Load each inode along the path until we find it */
    for (i = 0; i < nelements; i++)
    {
        if (strcmp(elements[i], "/") == 0)
        {
            current_ino = EXT2_ROOT_INO;
        }
        else
        {
            ext2_dir_ent_t* entries = NULL;
            uint32_t nentries = 0;
            uint32_t j;

            if ((err = ext2_list_dir_inode(
                ext2,
                current_ino,
                &entries,
                &nentries)))
            {
                GOTO(done);
            }

            current_ino = 0;

            for (j = 0; j < nentries; j++)
            {
                const ext2_dir_ent_t* ent = &entries[j];

                if (i + 1 == nelements || ent->d_type == EXT2_DT_DIR)
                {
                    if (strcmp(ent->d_name, elements[i]) == 0)
                    {
                        current_ino = ent->d_ino;
                        break;
                    }
                }
            }

            if (entries)
                free(entries);

            if (!current_ino)
            {
                /* Not found case */
                err = EXT2_ERR_FILE_NOT_FOUND;
                goto done;
            }
        }
    }

    *ino = current_ino;

    err = EXT2_ERR_NONE;

done:
    return err;
}

ext2_err_t ext2_path_to_inode(
    const ext2_t* ext2,
    const char* path,
    ext2_ino_t* ino,
    ext2_inode_t* inode)
{
    EXT2_DECLARE_ERR(err);
    ext2_ino_t tmp_ino;

    /* Check parameters */
    if (!ext2 || !path || !inode)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    /* Find the ino for this path */
    if ((err = ext2_path_to_ino(ext2, path, &tmp_ino)))
    {
        /* Not found case */
        goto done;
    }

    /* Read the inode into memory */
    if ((err = ext2_read_inode(ext2, tmp_ino, inode)))
    {
        GOTO(done);
    }

    if (ino)
        *ino = tmp_ino;

    err = EXT2_ERR_NONE;

done:
    return err;
}

/*
**==============================================================================
**
** bitmaps:
**
**==============================================================================
*/

ext2_err_t ext2_read_block_bitmap(
    const ext2_t* ext2,
    uint32_t group_index,
    ext2_block_t* block)
{
    EXT2_DECLARE_ERR(err);
    uint32_t bitmap_size_bytes;

    if (!ext2_valid(ext2) || !block)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    memset(block, 0, sizeof(ext2_block_t));

    bitmap_size_bytes = ext2->sb.s_blocks_per_group / 8;

    if (group_index > ext2->group_count)
    {
        err = EXT2_ERR_BAD_GRPNO;
        GOTO(done);
    }

    if ((err = ext2_read_block(
        ext2,
        ext2->groups[group_index].bg_block_bitmap,
        block)))
    {
        GOTO(done);
    }

    if (block->size > bitmap_size_bytes)
        block->size = bitmap_size_bytes;

    err = EXT2_ERR_NONE;

done:
    return err;
}

ext2_err_t ext2_write_block_bitmap(
    const ext2_t* ext2,
    uint32_t group_index,
    const ext2_block_t* block)
{
    EXT2_DECLARE_ERR(err);
    uint32_t bitmap_size_bytes;

    if (!ext2_valid(ext2) || !block)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    bitmap_size_bytes = ext2->sb.s_blocks_per_group / 8;

    if (block->size != bitmap_size_bytes)
    {
        err = EXT2_ERR_BAD_SIZE;
        GOTO(done);
    }

    if (group_index > ext2->group_count)
    {
        err = EXT2_ERR_BAD_GRPNO;
        GOTO(done);
    }

    if ((err = ext2_write_block(
        ext2,
        ext2->groups[group_index].bg_block_bitmap,
        block)))
    {
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:
    return err;
}

ext2_err_t ext2_read_inode_bitmap(
    const ext2_t* ext2,
    uint32_t group_index,
    ext2_block_t* block)
{
    EXT2_DECLARE_ERR(err);
    uint32_t bitmap_size_bytes;

    if (!ext2_valid(ext2) || !block)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    memset(block, 0, sizeof(ext2_block_t));

    bitmap_size_bytes = ext2->sb.s_inodes_per_group / 8;

    if (group_index > ext2->group_count)
    {
        err = EXT2_ERR_BAD_GRPNO;
        GOTO(done);
    }

    if ((err = ext2_read_block(
        ext2,
        ext2->groups[group_index].bg_inode_bitmap,
        block)))
    {
        GOTO(done);
    }

    if (block->size > bitmap_size_bytes)
        block->size = bitmap_size_bytes;

    err = EXT2_ERR_NONE;

done:
    return err;
}

/*
**==============================================================================
**
** directory:
**
**==============================================================================
*/

static void _dump_dir_entry(const ext2_dir_entry_t* dirent)
{
    printf("=== ext2_dir_entry_t:\n");
    printf("inode=%u\n", dirent->inode);
    printf("rec_len=%u\n", dirent->rec_len);
    printf("name_len=%u\n", dirent->name_len);
    printf("file_type=%u\n", dirent->file_type);
    printf("name={%.*s}\n", dirent->name_len, dirent->name);
}

static ext2_err_t _count_dir_entries(
    const ext2_t* ext2,
    const void* data,
    uint32_t size,
    uint32_t* count)
{
    EXT2_DECLARE_ERR(err);
    const uint8_t* p = (uint8_t*)data;
    const uint8_t* end = (uint8_t*)data + size;

    /* Check parameters */
    if (!ext2_valid(ext2) || !data || !size || !count)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    /* Initilize the count */
    *count = 0;

    /* Must be divisiable by block size */
    if ((end - p) % ext2->block_size)
    {
        err = EXT2_ERR_BAD_MULTIPLE;
        GOTO(done);
    }

    while (p < end)
    {
        const ext2_dir_entry_t* ent = (const ext2_dir_entry_t*)p;

        if (ent->name_len)
        {
            (*count)++;
        }

        p += ent->rec_len;
    }

    if (p != end)
    {
        err = EXT2_ERR_EXTRANEOUS_DATA;
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:
    return err;
}

struct _ext2_dir
{
    void *data;
    uint32_t size;
    const void *next;
    ext2_dir_ent_t ent;
};

ext2_dir_t *ext2_open_dir(const ext2_t* ext2, const char *path)
{
    ext2_dir_t *dir = NULL;
    ext2_ino_t ino;

    /* Check parameters */
    if (!ext2 || !path)
    {
        GOTO(done);
    }

    /* Find inode number for this directory */
    if ((ext2_path_to_ino(ext2, path, &ino)))
        GOTO(done);

    /* Open directory from this inode */
    if (!(dir = ext2_open_dir_ino(ext2, ino)))
        GOTO(done);

done:
    return dir;
}

ext2_dir_t *ext2_open_dir_ino(const ext2_t* ext2, ext2_ino_t ino)
{
    ext2_dir_t *dir = NULL;
    ext2_inode_t inode;

    /* Check parameters */
    if (!ext2 || !ino)
    {
        GOTO(done);
    }

    /* Read the inode into memory */
    if ((ext2_read_inode(ext2, ino, &inode)))
    {
        GOTO(done);
    }

    /* Fail if not a directory */
    if (!(inode.i_mode & EXT2_S_IFDIR))
    {
        GOTO(done);
    }

    /* Allocate directory object */
    if (!(dir = (ext2_dir_t*)calloc(1, sizeof(ext2_dir_t))))
    {
        GOTO(done);
    }

    /* Load the blocks for this inode into memory */
    if ((ext2_load_file_from_inode(
        ext2,
        &inode,
        &dir->data,
        &dir->size)))
    {
        free(dir);
        dir = NULL;
        GOTO(done);
    }

    /* Set pointer to current directory */
    dir->next = dir->data;

done:
    return dir;
}

ext2_dir_ent_t *ext2_read_dir(ext2_dir_t *dir)
{
    ext2_dir_ent_t *ent = NULL;

    if (!dir || !dir->data || !dir->next)
        goto done;

    /* Find the next entry (possibly skipping padding entries) */
    {
        const void* end = (void*) ((char*) dir->data + dir->size);

        while (!ent && dir->next < end)
        {
            const ext2_dir_entry_t* de =
                (ext2_dir_entry_t*)dir->next;

            if (de->rec_len == 0)
                break;

            if (de->name_len > 0)
            {
                /* Found! */

                /* Set ext2_dir_ent_t.d_ino */
                dir->ent.d_ino = de->inode;

                /* Set ext2_dir_ent_t.d_off (not used) */
                dir->ent.d_off = 0;

                /* Set ext2_dir_ent_t.d_reclen (not used) */
                dir->ent.d_reclen = sizeof(ext2_dir_ent_t);

                /* Set ext2_dir_ent_t.type */
                switch (de->file_type)
                {
                    case EXT2_FT_UNKNOWN:
                        dir->ent.d_type = EXT2_DT_UNKNOWN;
                        break;
                    case EXT2_FT_REG_FILE:
                        dir->ent.d_type = EXT2_DT_REG;
                        break;
                    case EXT2_FT_DIR:
                        dir->ent.d_type = EXT2_DT_DIR;
                        break;
                    case EXT2_FT_CHRDEV:
                        dir->ent.d_type = EXT2_DT_CHR;
                        break;
                    case EXT2_FT_BLKDEV:
                        dir->ent.d_type = EXT2_DT_BLK;
                        break;
                    case EXT2_FT_FIFO:
                        dir->ent.d_type = EXT2_DT_FIFO;
                        break;
                    case EXT2_FT_SOCK:
                        dir->ent.d_type = EXT2_DT_SOCK;
                        break;
                    case EXT2_FT_SYMLINK:
                        dir->ent.d_type = EXT2_DT_LNK;
                        break;
                    default:
                        dir->ent.d_type = EXT2_DT_UNKNOWN;
                        break;
                }

                /* Set ext2_dir_ent_t.d_name */
                dir->ent.d_name[0] = '\0';

                _strncat(
                    dir->ent.d_name,
                    sizeof(dir->ent.d_name),
                    de->name,
                    _min(EXT2_PATH_MAX-1, de->name_len));

                /* Success! */
                ent = &dir->ent;
            }

            /* Position to the next entry (for next call to readdir) */
                dir->next = (void*)((char*)dir->next + de->rec_len);
        }
    }

done:
    return ent;
}

ext2_err_t ext2_close_dir(ext2_dir_t* dir)
{
    EXT2_DECLARE_ERR(err);

    if (!dir)
    {
        GOTO(done);
    }

    free(dir->data);
    free(dir);

    err = EXT2_ERR_NONE;

done:
    return err;
}

ext2_err_t ext2_list_dir_inode(
    const ext2_t* ext2,
    ext2_ino_t ino,
    ext2_dir_ent_t** entries,
    uint32_t* nentries)
{
    EXT2_DECLARE_ERR(err);
    ext2_dir_t* dir = NULL;
    ext2_dir_ent_t* ent;
    buf_t buf = BUF_INITIALIZER;

    /* Check parameters */
    if (!ext2 || !ino || !entries || !nentries)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    *entries = NULL;
    *nentries = 0;

    /* Open the directory */
    if (!(dir = ext2_open_dir_ino(ext2, ino)))
    {
        GOTO(done);
    }

    /* Add entries to array */
    while ((ent = ext2_read_dir(dir)))
    {
        /* Append to buffer */
        if ((err = buf_append(
            &buf,
            ent,
            sizeof(ext2_dir_ent_t))))
        {
            GOTO(done);
        }
    }

    (*entries) = (ext2_dir_ent_t*)buf.data;
    (*nentries) = buf.size / sizeof(ext2_dir_ent_t);

    err = EXT2_ERR_NONE;

done:

    if ((err))
        buf_release(&buf);

    /* Close the directory */
    if (dir)
        ext2_close_dir(dir);

    return err;
}

/*
**==============================================================================
**
** files:
**
**==============================================================================
*/

static ext2_err_t _split_full_path(
    const char* path,
    char dirname[EXT2_PATH_MAX],
    char basename[EXT2_PATH_MAX])
{
    char* slash;

    /* Reject paths that are not absolute */
    if (path[0] != '/')
        return EXT2_ERR_FAILED;

    /* Handle root directory up front */
    if (strcmp(path, "/") == 0)
    {
        _strlcpy(dirname, "/", EXT2_PATH_MAX);
        _strlcpy(basename, "/", EXT2_PATH_MAX);
        return EXT2_ERR_NONE;
    }

    /* This cannot fail (prechecked) */
    if (!(slash = strrchr(path, '/')))
        return EXT2_ERR_FAILED;

    /* If path ends with '/' character */
    if (!slash[1])
        return EXT2_ERR_FAILED;

    /* Split the path */
    {
        if (slash == path)
        {
            _strlcpy(dirname, "/", EXT2_PATH_MAX);
        }
        else
        {
            uint64_t index = slash - path;
            _strlcpy(dirname, path, EXT2_PATH_MAX);

            if (index < EXT2_PATH_MAX)
                dirname[index] = '\0';
            else
                dirname[EXT2_PATH_MAX-1] = '\0';
        }

        _strlcpy(basename, slash + 1, EXT2_PATH_MAX);
    }

    return EXT2_ERR_NONE;
}

ext2_err_t ext2_load_file_from_inode(
    const ext2_t* ext2,
    const ext2_inode_t* inode,
    void** data,
    uint32_t* size)
{
    EXT2_DECLARE_ERR(err);
    buf_u32_t blknos = BUF_U32_INITIALIZER;
    buf_t buf = BUF_INITIALIZER;
    uint32_t i;

    /* Check parameters */
    if (!ext2_valid(ext2) || !inode || !data || !size)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    /* Initialize the output */
    *data = NULL;
    *size = 0;

    /* Form a list of block-numbers for this file */
    if ((err = _load_block_numbers_from_inode(
        ext2,
        inode,
        0, /* include_block_blocks */
        &blknos)))
    {
        GOTO(done);
    }

    /* Read and append each block */
    for (i = 0; i < blknos.size; )
    {
        uint32_t nblks = 1;
        uint32_t j;
        ext2_block_t block;

        /* Count the number of consecutive blocks: nblks */
        for (j = i + 1; j < blknos.size; j++)
        {
            if (blknos.data[j] != blknos.data[j-1] + 1)
                break;

            nblks++;
        }

        if (nblks == 1)
        {
            /* Read the next block */
            if ((err = ext2_read_block(ext2, blknos.data[i], &block)))
            {
                GOTO(done);
            }

            /* Append block to end of buffer */
            if ((err = buf_append(&buf, block.data, block.size)))
                GOTO(done);
        }
        else
        {
            if ((_read_blocks(ext2, blknos.data[i], nblks, &buf)))
            {
                GOTO(done);
            }
        }

        i += nblks;
    }

    *data = buf.data;
    *size = inode->i_size; /* data may be smaller than block multiple */

    err = EXT2_ERR_NONE;

done:

    if ((err))
        buf_release(&buf);

    buf_u32_release(&blknos);

    return err;
}

ext2_err_t ext2_load_file_from_path(
    const ext2_t* ext2,
    const char* path,
    void** data,
    uint32_t* size)
{
    EXT2_DECLARE_ERR(err);
    ext2_inode_t inode;

    if (ext2_path_to_inode(ext2, path, NULL, &inode) != EXT2_ERR_NONE)
        goto done;

    if (ext2_load_file_from_inode(ext2, &inode, data, size) != EXT2_ERR_NONE)
        goto done;

    err = EXT2_ERR_NONE;

done:
    return err;
}

/*
**==============================================================================
**
** ext2_t
**
**==============================================================================
*/

ext2_err_t ext2_new(blkdev_t* dev, ext2_t** ext2_out)
{
    EXT2_DECLARE_ERR(err);
    ext2_t* ext2 = NULL;

    /* Check parameters */
    if (!dev || !ext2_out)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    /* Initialize output parameters */
    *ext2_out = NULL;

    /* Bit endian is not supported */
    if (_is_big_endian())
    {
        err = EXT2_ERR_UNSUPPORTED;
        GOTO(done);
    }

    /* Allocate the file system object */
    if (!(ext2 = (ext2_t*)calloc(1, sizeof(ext2_t))))
    {
        err = EXT2_ERR_OUT_OF_MEMORY;
        GOTO(done);
    }

    /* Set the file object */
    ext2->dev = dev;

    /* Read the superblock */
    if ((err = _read_super_block(ext2->dev, &ext2->sb)))
    {
        err = EXT2_ERR_FAILED_TO_READ_SUPERBLOCK;
        GOTO(done);
    }

    /* Check the superblock magic number */
    if (ext2->sb.s_magic != EXT2_S_MAGIC)
    {
        err = EXT2_ERR_BAD_MAGIC;
        GOTO(done);
    }

    /* Reject revision 0 file systems */
    if (ext2->sb.s_rev_level == EXT2_GOOD_OLD_REV)
    {
        /* Revision 0 not supported */
        err = EXT2_ERR_UNSUPPORTED_REVISION;
        GOTO(done);
    }

    /* Accept revision 1 file systems */
    if (ext2->sb.s_rev_level < EXT2_DYNAMIC_REV)
    {
        /* Revision 1 and up supported */
        err = EXT2_ERR_UNSUPPORTED_REVISION;
        GOTO(done);
    }

    /* Check inode size */
    if (ext2->sb.s_inode_size > sizeof(ext2_inode_t))
    {
        err = EXT2_ERR_UNEXPECTED;
        GOTO(done);
    }

    /* Calcualte the block size in bytes */
    ext2->block_size = 1024 << ext2->sb.s_log_block_size;

    /* Calculate the number of block groups */
    ext2->group_count =
        1 + (ext2->sb.s_blocks_count-1) / ext2->sb.s_blocks_per_group;

    /* Get the groups list */
    if (!(ext2->groups = _read_groups(ext2)))
    {
        err = EXT2_ERR_FAILED_TO_READ_GROUPS;
        GOTO(done);
    }

    /* Read the root inode */
    if ((err = ext2_read_inode(ext2, EXT2_ROOT_INO, &ext2->root_inode)))
    {
        err = EXT2_ERR_FAILED_TO_READ_INODE;
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:

    if ((err))
    {
        /* Caller must dispose of this! */
        ext2->dev = NULL;
        ext2_delete(ext2);
    }
    else
        *ext2_out = ext2;

    return err;
}

void ext2_delete(ext2_t* ext2)
{
    if (ext2)
    {
        if (ext2->dev)
            ext2->dev->close(ext2->dev);

        if (ext2->groups)
            free(ext2->groups);

        free(ext2);
    }
}

ext2_err_t ext2_dump(const ext2_t* ext2)
{
    EXT2_DECLARE_ERR(err);
    uint32_t grpno;

    /* Print the superblock */
    ext2_dump_super_block(&ext2->sb);

    printf("block_size=%u\n", ext2->block_size);
    printf("group_count=%u\n", ext2->group_count);

    /* Print the groups */
    _dump_group_descs(ext2->groups, ext2->group_count);

    /* Print out the bitmaps for the data blocks */
    {
        for (grpno = 0; grpno < ext2->group_count; grpno++)
        {
            ext2_block_t bitmap;

            if ((err = ext2_read_block_bitmap(ext2, grpno, &bitmap)))
            {
                GOTO(done);
            }

            printf("=== block bitmap:\n");
            _dump_bitmap(&bitmap);
        }
    }

    /* Print the inode bitmaps */
    for (grpno = 0; grpno < ext2->group_count; grpno++)
    {
        ext2_block_t bitmap;

        if ((err = ext2_read_inode_bitmap(ext2, grpno, &bitmap)))
        {
            GOTO(done);
        }

        printf("=== inode bitmap:\n");
        _dump_bitmap(&bitmap);
    }

    /* dump the inodes */
    {
        uint32_t nbits = 0;
        uint32_t mbits = 0;

        /* Print the inode tables */
        for (grpno = 0; grpno < ext2->group_count; grpno++)
        {
            ext2_block_t bitmap;
            uint32_t lino;

            /* Get inode bitmap for this group */
            if ((err = ext2_read_inode_bitmap(ext2, grpno, &bitmap)))
            {
                GOTO(done);
            }

            nbits += _count_bits_n(bitmap.data, bitmap.size);

            /* For each bit set in the bit map */
            for (lino = 0; lino < ext2->sb.s_inodes_per_group; lino++)
            {
                ext2_inode_t inode;
                ext2_ino_t ino;

                if (!_test_bit(bitmap.data, bitmap.size, lino))
                    continue;

                mbits++;

                if ((lino+1) < EXT2_FIRST_INO && (lino+1) != EXT2_ROOT_INO)
                    continue;

                ino = _make_ino(ext2, grpno, lino);

                if ((err = ext2_read_inode(ext2, ino, &inode)))
                {
                    GOTO(done);
                }

                printf("INODE{%u}\n", ino);
                ext2_dump_inode(ext2, &inode);
            }
        }

        printf("nbits{%u}\n", nbits);
        printf("mbits{%u}\n", mbits);
    }

    /* dump the root inode */
    ext2_dump_inode(ext2, &ext2->root_inode);

    err = EXT2_ERR_NONE;

done:
    return err;
}

ext2_err_t ext2_check(const ext2_t* ext2)
{
    EXT2_DECLARE_ERR(err);

    /* Check the block bitmaps */
    {
        uint32_t i;
        uint32_t n = 0;
        uint32_t nused = 0;
        uint32_t nfree = 0;

        for (i = 0; i < ext2->group_count; i++)
        {
            ext2_block_t bitmap;

            nfree += ext2->groups[i].bg_free_blocks_count;

            if ((err = ext2_read_block_bitmap(ext2, i, &bitmap)))
            {
                GOTO(done);
            }

            nused += _count_bits_n(bitmap.data, bitmap.size);
            n += bitmap.size * 8;
        }

        if (ext2->sb.s_free_blocks_count != nfree)
        {
            printf("s_free_blocks_count{%u}, nfree{%u}\n",
                ext2->sb.s_free_blocks_count, nfree);
            GOTO(done);
        }

        if (ext2->sb.s_free_blocks_count != n - nused)
        {
            GOTO(done);
        }
    }

    /* Check the inode bitmaps */
    {
        uint32_t i;
        uint32_t n = 0;
        uint32_t nused = 0;
        uint32_t nfree = 0;

        /* Check the bitmaps for the inodes */
        for (i = 0; i < ext2->group_count; i++)
        {
            ext2_block_t bitmap;

            nfree += ext2->groups[i].bg_free_inodes_count;

            if ((err = ext2_read_inode_bitmap(ext2, i, &bitmap)))
            {
                GOTO(done);
            }

            nused += _count_bits_n(bitmap.data, bitmap.size);
            n += bitmap.size * 8;
        }

        if (ext2->sb.s_free_inodes_count != n - nused)
        {
            GOTO(done);
        }

        if (ext2->sb.s_free_inodes_count != nfree)
        {
            GOTO(done);
        }
    }

    /* Check the inodes */
    {
        uint32_t grpno;
        uint32_t nbits = 0;
        uint32_t mbits = 0;

        /* Check the inode tables */
        for (grpno = 0; grpno < ext2->group_count; grpno++)
        {
            ext2_block_t bitmap;
            uint32_t lino;

            /* Get inode bitmap for this group */
            if ((err = ext2_read_inode_bitmap(ext2, grpno, &bitmap)))
            {
                GOTO(done);
            }

            nbits += _count_bits_n(bitmap.data, bitmap.size);

            /* For each bit set in the bit map */
            for (lino = 0; lino < ext2->sb.s_inodes_per_group; lino++)
            {
                ext2_inode_t inode;
                ext2_ino_t ino;

                if (!_test_bit(bitmap.data, bitmap.size, lino))
                    continue;

                mbits++;

                if ((lino+1) < EXT2_FIRST_INO && (lino+1) != EXT2_ROOT_INO)
                    continue;

                ino = _make_ino(ext2, grpno, lino);

                if ((err = ext2_read_inode(ext2, ino, &inode)))
                {
                    GOTO(done);
                }

                /* Mode can never be zero */
                if (inode.i_mode == 0)
                {
                    GOTO(done);
                }

                /* If file is not zero size but no blocks, then fail */
                if (inode.i_size && !inode.i_block[0])
                {
                    GOTO(done);
                }
            }
        }

        /* The number of bits in bitmap must match number of active inodes */
        if (nbits != mbits)
        {
            GOTO(done);
        }
    }

    err = EXT2_ERR_NONE;

done:
    return err;
}

ext2_err_t ext2_trunc(ext2_t* ext2, const char* path)
{
    EXT2_DECLARE_ERR(err);
    char dirname[EXT2_PATH_MAX];
    char basename[EXT2_PATH_MAX];
    ext2_ino_t dir_ino;
    ext2_inode_t dir_inode;
    ext2_ino_t file_ino;
    ext2_inode_t file_inode;
    buf_u32_t blknos = BUF_U32_INITIALIZER;
    (void)_dump_block_numbers;

    /* Check parameters */
    if (!ext2 || !path)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    /* Split the path */
    if ((err = _split_full_path(path, dirname, basename)))
    {
        GOTO(done);
    }

    /* Find the inode of the dirname */
    if ((err = ext2_path_to_inode(ext2, dirname, &dir_ino, &dir_inode)))
    {
        GOTO(done);
    }

    /* Find the inode of the basename */
    if ((err = ext2_path_to_inode(
        ext2, path, &file_ino, &file_inode)))
    {
        goto done;
    }

    /* Form a list of block-numbers for this file */
    if ((err = _load_block_numbers_from_inode(
        ext2,
        &file_inode,
        1, /* include_block_blocks */
        &blknos)))
    {
        GOTO(done);
    }

    /* If this file is a directory, then fail */
    if (file_inode.i_mode & EXT2_S_IFDIR)
    {
        GOTO(done);
    }

    /* Return the blocks to the free list */
    {
        if ((err = _put_blocks(ext2, blknos.data, blknos.size)))
            GOTO(done);
    }

    /* Rewrite the inode */
    {
        file_inode.i_size = 0;
        memset(file_inode.i_block, 0, sizeof(file_inode.i_block));

        if ((err = _write_inode(ext2, file_ino, &file_inode)))
        {
            GOTO(done);
        }
    }

    /* Update the super block */
    if ((err = _write_super_block(ext2)))
    {
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:

    buf_u32_release(&blknos);

    return err;
}

static ext2_err_t _write_single_direct_block_numbers(
    ext2_t* ext2,
    const uint32_t* blknos,
    uint32_t nblknos,
    uint32_t* blkno)
{
    EXT2_DECLARE_ERR(err);
    ext2_block_t block;

    /* Check parameters */
    if (!ext2_valid(ext2) || !blknos || !nblknos)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    /* If no room in a single block for the block numbers */
    if (nblknos > ext2->block_size / sizeof(uint32_t))
    {
        err = EXT2_ERR_BUFFER_OVERFLOW;
        GOTO(done);
    }

    /* Assign an available block */
    if ((err = _get_block(ext2, blkno)))
    {
        GOTO(done);
    }

    /* Copy block numbers into block */
    memset(&block, 0, sizeof(ext2_block_t));
    memcpy(block.data, blknos, nblknos * sizeof(uint32_t));
    block.size = ext2->block_size;

    /* Write the block */
    if ((err = ext2_write_block(ext2, *blkno, &block)))
    {
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:
    return err;
}

static ext2_err_t _write_indirect_block_numbers(
    ext2_t* ext2,
    uint32_t indirection, /* level of indirection: 2=double, 3=triple */
    const uint32_t* blknos,
    uint32_t nblknos,
    uint32_t* blkno)
{
    EXT2_DECLARE_ERR(err);
    ext2_block_t block;
    uint32_t blknos_per_block = ext2->block_size / sizeof(uint32_t);

    /* Check parameters */
    if (!ext2_valid(ext2) || !blknos || !nblknos || !blkno)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    /* Check indirection level */
    if (indirection != 2 && indirection != 3)
    {
        err = EXT2_ERR_UNEXPECTED;
        GOTO(done);
    }

    /* If double indirection */
    if (indirection == 2)
    {
        /* If too many indirect blocks */
        if (nblknos > blknos_per_block * blknos_per_block)
        {
            err = EXT2_ERR_BUFFER_OVERFLOW;
            GOTO(done);
        }
    }

    /* Assign an available block */
    if ((err = _get_block(ext2, blkno)))
    {
        GOTO(done);
    }

    /* Allocate indirect block (to hold block numbers) */
    memset(&block, 0, sizeof(ext2_block_t));
    block.size = ext2->block_size;

    /* Write each of the indirect blocks */
    {
        const uint32_t* p = blknos;
        uint32_t r = nblknos;
        uint32_t i;

        /* For each block */
        for (i = 0; r > 0; i++)
        {
            uint32_t n;

            /* If double indirection */
            if (indirection == 2)
            {
                n = _min(r, blknos_per_block);

                if ((err = _write_single_direct_block_numbers(
                    ext2,
                    p,
                    n,
                    (uint32_t*)block.data + i)))
                {
                    GOTO(done);
                }
            }
            else
            {
                n = _min(r, blknos_per_block * blknos_per_block);

                /* Write the block numbers for this block */
                if ((err = _write_indirect_block_numbers(
                    ext2,
                    2, /* double indirection */
                    p,
                    n,
                    (uint32_t*)block.data + i)))
                {
                    GOTO(done);
                }
            }

            p += n;
            r -= n;
        }

        /* Check that the blocks were exhausted */
        if (r != 0)
        {
            err = EXT2_ERR_EXTRANEOUS_DATA;
            GOTO(done);
        }
    }

    /* Write the indirect block */
    if ((err = ext2_write_block(ext2, *blkno, &block)))
    {
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:
    return err;
}

static ext2_err_t _write_data(
    ext2_t* ext2,
    const void* data,
    uint32_t size,
    buf_u32_t* blknos)
{
    EXT2_DECLARE_ERR(err);
    uint32_t blksize = ext2->block_size;
    uint32_t nblks = (size + blksize - 1) / blksize;
    uint32_t rem = size % blksize;
    uint32_t i = 0;
    uint32_t grpno;

    for (grpno = 0; grpno < ext2->group_count && i < nblks; grpno++)
    {
        ext2_block_t bitmap;
        ext2_block_t block;
        uint32_t blkno;
        uint32_t lblkno;
        int changed = 0;

        /* Read the bitmap */
        if ((err = ext2_read_block_bitmap(ext2, grpno, &bitmap)))
        {
            GOTO(done);
        }

        /* Scan the bitmap, looking for free bit */
        for (lblkno = 0; lblkno < bitmap.size * 8 && i < nblks; lblkno++)
        {
            if (!_test_bit(bitmap.data, bitmap.size, lblkno))
            {
                _set_bit(bitmap.data, bitmap.size, lblkno);
                blkno = _make_blkno(ext2, grpno, lblkno);

                /* Append this to the array of blocks */
                if ((err = buf_u32_append(blknos, &blkno, 1)))
                    GOTO(done);

                /* If this is the final block */
                if (i + 1 == nblks && rem)
                {
                    memcpy(block.data, (char*)data + (i * blksize), rem);
                    block.size = rem;
                }
                else
                {
                    memcpy(block.data, (char*)data + (i * blksize), blksize);
                    block.size = blksize;
                }

                /* Write the block */
                if ((err = ext2_write_block(ext2, blkno, &block)))
                {
                    GOTO(done);
                }

                /* Update the in memory structs. */
                ext2->sb.s_free_blocks_count--;
                ext2->groups[grpno].bg_free_blocks_count--;
                i++;
                changed = 1;
            }
        }

        if (changed == 1)
        {
            /* Write group and bitmap. */
            if ((err = _write_group(ext2, grpno)))
            {
                GOTO(done);
            }

            if ((err = ext2_write_block_bitmap(ext2, grpno, &bitmap)))
            {
                GOTO(done);
            }
        }
    }

    /* Ran out of disk space. */
    if (i != nblks)
    {
        GOTO(done);
    }

    /* Write super block data. */
    if ((err = _write_super_block(ext2)))
    {
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:
    return err;
}

static ext2_err_t _update_inode_block_pointers(
    ext2_t* ext2,
    ext2_ino_t ino,
    ext2_inode_t* inode,
    uint32_t size,
    const uint32_t* blknos,
    uint32_t nblknos)
{
    EXT2_DECLARE_ERR(err);
    uint32_t i;
    const uint32_t* p = blknos;
    uint32_t r = nblknos;
    uint32_t blknos_per_block = ext2->block_size / sizeof(uint32_t);

    /* Update the inode size */
    inode->i_size = size;

    /* Update the direct inode blocks */
    if (r)
    {
        uint32_t n = _min(r, EXT2_SINGLE_INDIRECT_BLOCK);

        for (i = 0; i < n; i++)
        {
            inode->i_block[i] = p[i];
        }

        p += n;
        r -= n;
    }

    /* Write the indirect block numbers */
    if (r)
    {
        uint32_t n = _min(r, blknos_per_block);

        if ((err = _write_single_direct_block_numbers(
            ext2,
            p,
            n,
            &inode->i_block[EXT2_SINGLE_INDIRECT_BLOCK])))
        {
            GOTO(done);
        }

        p += n;
        r -= n;
    }

    /* Write the double indirect block numbers */
    if (r)
    {
        uint32_t n = _min(r, blknos_per_block * blknos_per_block);

        if ((err = _write_indirect_block_numbers(
            ext2,
            2, /* double indirection */
            p,
            n,
            &inode->i_block[EXT2_DOUBLE_INDIRECT_BLOCK])))
        {
            GOTO(done);
        }

        p += n;
        r -= n;
    }

    /* Write the triple indirect block numbers */
    if (r)
    {
        uint32_t n = r;

        if ((err = _write_indirect_block_numbers(
            ext2,
            3, /* triple indirection */
            p,
            n,
            &inode->i_block[EXT2_TRIPLE_INDIRECT_BLOCK])))
        {
            GOTO(done);
        }

        p += n;
        r -= n;
    }

    /* Note: triple indirect block numbers not handled! */
    if (r > 0)
    {
        GOTO(done);
    }

    /* Rewrite the inode */
    if ((err = _write_inode(ext2, ino, inode)))
    {
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:
    return err;
}

static ext2_err_t _update_inode_data_blocks(
    ext2_t* ext2,
    ext2_ino_t ino,
    ext2_inode_t* inode,
    const void* data,
    uint32_t size,
    bool is_dir)
{
    EXT2_DECLARE_ERR(err);
    buf_u32_t blknos = BUF_U32_INITIALIZER;
    uint32_t count = 0;
    void* tmp_data = NULL;

    if ((err = _write_data(ext2, data, size, &blknos)))
        GOTO(done);

    if (is_dir)
    {
        if ((err = _count_dir_entries(
            ext2, data, size, &count)))
        {
            GOTO(done);
        }
    }

    inode->i_osd1 = count + 1;

    if ((err = _update_inode_block_pointers(
        ext2,
        ino,
        inode,
        size,
        blknos.data,
        blknos.size)))
    {
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:

    buf_u32_release(&blknos);

    if (tmp_data)
        free(tmp_data);

    return err;
}

ext2_err_t ext2_update(
    ext2_t* ext2,
    const void* data,
    uint32_t size,
    const char* path)
{
    EXT2_DECLARE_ERR(err);
    ext2_ino_t ino;
    ext2_inode_t inode;
    (void)_dump_dir_entry;

    /* Check parameters */
    if (!ext2_valid(ext2) || !data || !path)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    /* Truncate the destination file */
    if ((err = ext2_trunc(ext2, path)))
    {
        GOTO(done);
    }

    /* Read inode for this file */
    if ((err = ext2_path_to_inode(ext2, path, &ino, &inode)))
    {
        GOTO(done);
    }

    /* Update the inode blocks */
    if ((err = _update_inode_data_blocks(
        ext2,
        ino,
        &inode,
        data,
        size,
        0))) /* !is_dir */
    {
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:

    return err;
}

static ext2_err_t _check_dir_entries(
    const ext2_t* ext2,
    const void* data,
    uint32_t size)
{
    EXT2_DECLARE_ERR(err);
    const uint8_t* p = (uint8_t*)data;
    const uint8_t* end = (uint8_t*)data + size;

    /* Must be divisiable by block size */
    if ((end - p) % ext2->block_size)
    {
        GOTO(done);
    }

    while (p < end)
    {
        uint32_t n;
        const ext2_dir_entry_t* ent = (const ext2_dir_entry_t*)p;

        n = sizeof(ext2_dir_entry_t) - EXT2_PATH_MAX + ent->name_len;
        n = _next_mult(n, 4);

        if (n != ent->rec_len)
        {
            uint32_t offset = ((char*)p - (char*)data) % ext2->block_size;
            uint32_t rem = ext2->block_size - offset;

            if (rem != ent->rec_len)
            {
                GOTO(done);
            }
        }

        p += ent->rec_len;
    }

    if (p != end)
    {
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:
    return err;
}

static void _dump_dir_entries(
    const ext2_t* ext2,
    const void* data,
    uint32_t size)
{
    const uint8_t* p = (uint8_t*)data;
    const uint8_t* end = (uint8_t*)data + size;

    while (p < end)
    {
        uint32_t n;
        const ext2_dir_entry_t* ent = (const ext2_dir_entry_t*)p;

        _dump_dir_entry(ent);

        n = sizeof(ext2_dir_entry_t) - EXT2_PATH_MAX + ent->name_len;
        n = _next_mult(n, 4);

        if (n != ent->rec_len)
        {
            uint32_t gap = ent->rec_len - n;
            uint32_t offset = ((char*)p - (char*)data) % ext2->block_size;
            uint32_t rem = ext2->block_size - offset;

            printf("gap: %u\n", gap);
            printf("offset: %u\n", offset);
            printf("remaing: %u\n", rem);
        }

        p += ent->rec_len;
    }
}

static const ext2_dir_entry_t* _find_dir_entry(
    const char* name,
    const void* data,
    uint32_t size)
{
    const uint8_t* p = (uint8_t*)data;
    const uint8_t* end = (uint8_t*)data + size;

    while (p < end)
    {
        const ext2_dir_entry_t* ent = (const ext2_dir_entry_t*)p;
        char tmp[EXT2_PATH_MAX];


        /* Create zero-terminated name */
        tmp[0] = '\0';
        _strncat(tmp, sizeof(tmp), ent->name,
            _min(ent->name_len, EXT2_PATH_MAX-1));

        if (strcmp(tmp, name) == 0)
            return ent;

        p += ent->rec_len;
    }

    /* Not found */
    return NULL;
}

static ext2_err_t _count_dir_entry_ino(
    ext2_t* ext2,
    ext2_ino_t ino,
    uint32_t* count)
{
    EXT2_DECLARE_ERR(err);
    ext2_dir_t* dir = NULL;
    ext2_dir_ent_t* ent;

    /* Check parameters */
    if (!ext2_valid(ext2) || !ino || !count)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    /* Initialize count */
    *count = 0;

    /* Open directory */
    if (!(dir = ext2_open_dir_ino(ext2, ino)))
    {
        GOTO(done);
    }

    /* Count the entries (notes that "." and ".." will always be present). */
    while ((ent = ext2_read_dir(dir)))
    {
        (*count)++;
    }

    err = EXT2_ERR_NONE;

done:

    if (dir)
        ext2_close_dir(dir);

    return err;
}

ext2_err_t ext2_rm(ext2_t* ext2, const char* path)
{
    EXT2_DECLARE_ERR(err);
    char dirname[EXT2_PATH_MAX];
    char filename[EXT2_PATH_MAX];
    void* blocks = NULL;
    uint32_t blocks_size = 0;
    void* new_blocks = NULL;
    uint32_t new_blocks_size = 0;
    buf_u32_t blknos = BUF_U32_INITIALIZER;
    ext2_ino_t ino;
    ext2_inode_t inode;
    const ext2_dir_entry_t* ent = NULL;

    (void)_dump_dir_entries;

    /* Check parameters */
    if (!ext2_valid(ext2) && !path)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    /* Truncate the file first */
    if ((err = ext2_trunc(ext2, path)))
    {
        goto done;
    }

    /* Split the path */
    if ((err = _split_full_path(path, dirname, filename)))
    {
        GOTO(done);
    }

    /* Load the directory inode */
    if ((err = ext2_path_to_inode(ext2, dirname, &ino, &inode)))
    {
        GOTO(done);
    }

    /* Load the directory file */
    if ((err = ext2_load_file_from_inode(
        ext2,
        &inode,
        &blocks,
        &blocks_size)))
    {
        GOTO(done);
    }

    /* Load the block numbers (including the block blocks) */
    if ((err = _load_block_numbers_from_inode(
        ext2,
        &inode,
        1, /* include_block_blocks */
        &blknos)))
    {
        GOTO(done);
    }

    /* Find 'filename' within this directory */
    if (!(ent = _find_dir_entry(filename, blocks, blocks_size)))
    {
        GOTO(done);
    }

    /* Allow removal of empty directories only */
    if (ent->file_type == EXT2_FT_DIR)
    {
        ext2_ino_t dir_ino;
        ext2_inode_t dir_inode;
        uint32_t count;

        /* Find the inode of the filename */
        if ((err = ext2_path_to_inode(
            ext2, filename, &dir_ino, &dir_inode)))
        {
            GOTO(done);
        }

        /* Disallow removal if directory is non empty */
        if ((err = _count_dir_entry_ino(ext2, dir_ino, &count)))
        {
            GOTO(done);
        }

        /* Expect just "." and ".." entries */
        if (count != 2)
        {
            GOTO(done);
        }
    }

    /* Convert from 'indexed' to 'linked list' directory format */
    {
        new_blocks_size = blocks_size;
        const char* src = (const char*)blocks;
        const char* src_end = (const char*)blocks + inode.i_size;
        char* dest = NULL;

        /* Allocate a buffer to hold 'linked list' directory */
        if (!(new_blocks = calloc(new_blocks_size, 1)))
        {
            GOTO(done);
        }

        /* Set current and end pointers to new buffer */
        dest = (char*)new_blocks;

        /* Copy over directory entries (skipping removed entry) */
        {
            ext2_dir_entry_t* prev = NULL;

            while (src < src_end)
            {
                const ext2_dir_entry_t* curr_ent =
                    (const ext2_dir_entry_t*)src;
                uint32_t rec_len;
                uint32_t offset;

                /* Skip the removed directory entry */
                if (curr_ent == ent || !ent->name)
                {
                    src += curr_ent->rec_len;
                    continue;
                }

                /* Compute size of the new directory entry */
                rec_len = sizeof(*curr_ent) - EXT2_PATH_MAX + curr_ent->name_len;
                rec_len = _next_mult(rec_len, 4);

                /* Compute byte offset into current block */
                offset = (dest - (char*)new_blocks) % ext2->block_size;

                /* If new entry would overflow the block */
                if (offset + rec_len > ext2->block_size)
                {
                    uint32_t rem = ext2->block_size - offset;

                    if (!prev)
                    {
                        GOTO(done);
                    }

                    /* Adjust previous entry to point to next block */
                    prev->rec_len += rem;
                    dest += rem;
                }

                /* Copy this entry into new buffer */
                {
                    ext2_dir_entry_t* new_ent =
                        (ext2_dir_entry_t*)dest;
                    memset(new_ent, 0, rec_len);
                    memcpy(new_ent, curr_ent,
                        sizeof(*curr_ent) + curr_ent->name_len);

                    new_ent->rec_len = rec_len;
                    prev = new_ent;
                    dest += rec_len;
                }

                src += curr_ent->rec_len;
            }

            /* Set final entry to point to end of the block */
            if (prev)
            {
                uint32_t offset;
                uint32_t rem;

                /* Compute byte offset into current block */
                offset = (dest - (char*)new_blocks) % ext2->block_size;

                /* Compute remaining bytes */
                rem = ext2->block_size - offset;

                /* Set record length of final entry to end of block */
                prev->rec_len += rem;

                /* Advance dest to block boundary */
                dest += rem;
            }

            /* Size down the new blocks size */
            new_blocks_size = (uint32_t)(dest - (char*)new_blocks);

            if ((err = _check_dir_entries(
                ext2,
                new_blocks,
                new_blocks_size)))
            {
                GOTO(done);
            }
        }
    }

    /* Count directory entries before and after */
    {
        uint32_t count;
        uint32_t new_count;

        if ((err = _count_dir_entries(
            ext2,
            blocks,
            blocks_size,
            &count)))
        {
            GOTO(done);
        }

        if ((err = _count_dir_entries(
            ext2,
            new_blocks,
            new_blocks_size,
            &new_count)))
        {
            GOTO(done);
        }
    }

    /* Return all directory blocks to the free list */
    if ((err = _put_blocks(ext2, blknos.data, blknos.size)))
    {
        GOTO(done);
    }

    /* Update the inode blocks */
    if ((err = _update_inode_data_blocks(
        ext2,
        ino,
        &inode,
        new_blocks,
        new_blocks_size,
        1))) /* is_dir */
    {
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:

    if (blocks)
        free(blocks);

    if (new_blocks)
        free(new_blocks);

    buf_u32_release(&blknos);

    return err;
}

static ext2_err_t _create_file_inode(
    ext2_t* ext2,
    const void* data,
    uint32_t size,
    uint16_t mode,
    uint32_t* blknos,
    uint32_t nblknos,
    uint32_t* ino)
{
    EXT2_DECLARE_ERR(err);
    ext2_inode_t inode;

    /* Check parameters */
    if (!ext2_valid(ext2) || !data || !blknos)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    /* Initialize the inode */
    {
        const uint32_t t = time(NULL);

        memset(&inode, 0, sizeof(ext2_inode_t));

        /* Set the mode of the new file */
        inode.i_mode = mode;

        /* Set the uid and gid to root */
        inode.i_uid = 0;
        inode.i_gid = 0;

        /* Set the size of this file */
        inode.i_size = size;

        /* Set the access, creation, and mtime to the same value */
        inode.i_atime = t;
        inode.i_ctime = t;
        inode.i_mtime = t;

        /* Linux-specific value */
        inode.i_osd1 = 1;

        /* The number of links is initially 1 */
        inode.i_links_count = 1;

        /* Set the number of 512 byte blocks */
        inode.i_blocks = (nblknos * ext2->block_size) / 512;
    }

    /* Assign an inode number */
    if ((err = _get_ino(ext2, ino)))
    {
        GOTO(done);
    }

    /* Update the inode block pointers and write the inode to disk */
    if ((err = _update_inode_block_pointers(
        ext2,
        *ino,
        &inode,
        size,
        blknos,
        nblknos)))
    {
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:
    return err;
}

typedef struct _ext2_dir_entry_buf
{
    ext2_dir_entry_t base;
    char buf[EXT2_PATH_MAX];
}
ext2_dir_ent_buf_t;

static ext2_err_t _create_dir_inode_and_block(
    ext2_t* ext2,
    ext2_ino_t parent_ino,
    uint16_t mode,
    ext2_ino_t* ino)
{
    EXT2_DECLARE_ERR(err);
    ext2_inode_t inode;
    uint32_t blkno;
    ext2_block_t block;

    /* Check parameters */
    if (!ext2_valid(ext2) || !mode || !parent_ino || !ino)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    /* Initialize the inode */
    {
        const uint32_t t = time(NULL);

        memset(&inode, 0, sizeof(ext2_inode_t));

        /* Set the mode of the new file */
        inode.i_mode = mode;

        /* Set the uid and gid to root */
        inode.i_uid = 0;
        inode.i_gid = 0;

        /* Set the size of this file */
        inode.i_size = ext2->block_size;

        /* Set the access, creation, and mtime to the same value */
        inode.i_atime = t;
        inode.i_ctime = t;
        inode.i_mtime = t;

        /* Linux-specific value */
        inode.i_osd1 = 1;

        /* The number of links is initially 2 */
        inode.i_links_count = 2;

        /* Set the number of 512 byte blocks */
        inode.i_blocks = ext2->block_size / 512;
    }

    /* Assign an inode number */
    if ((err = _get_ino(ext2, ino)))
    {
        GOTO(done);
    }

    /* Assign a block number */
    if ((err = _get_block(ext2, &blkno)))
    {
        GOTO(done);
    }

    /* Create a block to hold the two directory entries */
    {
        ext2_dir_ent_buf_t dot1;
        ext2_dir_ent_buf_t dot2;
        ext2_dir_entry_t* ent;

        /* The "." directory */
        memset(&dot1, 0, sizeof(dot1));
        dot1.base.inode = *ino;
        dot1.base.name_len = 1;
        dot1.base.file_type = EXT2_DT_DIR;
        dot1.base.name[0] = '.';
        uint16_t d1len = sizeof(ext2_dir_entry_t) - EXT2_PATH_MAX + dot1.base.name_len;
        dot1.base.rec_len = _next_mult(d1len, 4);

        /* The ".." directory */
        memset(&dot2, 0, sizeof(dot2));
        dot2.base.inode = parent_ino;
        dot2.base.name_len = 2;
        dot2.base.file_type = EXT2_DT_DIR;
        dot2.base.name[0] = '.';
        dot2.base.name[1] = '.';
        uint16_t d2len = sizeof(ext2_dir_entry_t) - EXT2_PATH_MAX + dot2.base.name_len;
        dot2.base.rec_len = _next_mult(d2len, 4);

        /* Initialize the directory entries block */
        memset(&block, 0, sizeof(ext2_block_t));
        memcpy(block.data, &dot1, dot1.base.rec_len);
        memcpy(block.data + dot1.base.rec_len, &dot2, dot2.base.rec_len);
        block.size = ext2->block_size;

        /* Adjust dot2.rec_len to point to end of block */
        ent = (ext2_dir_entry_t*)(block.data + dot1.base.rec_len);

        ent->rec_len += ext2->block_size -
            (dot1.base.rec_len + dot2.base.rec_len);

        /* Write the block */
        if ((err = ext2_write_block(ext2, blkno, &block)))
        {
            GOTO(done);
        }
    }

    /* Update the inode block pointers and write the inode to disk */
    if ((err = _update_inode_block_pointers(
        ext2,
        *ino,
        &inode,
        ext2->block_size,
        &blkno,
        1)))
    {
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:
    return err;
}

static ext2_err_t _append_dir_entry(
    ext2_t* ext2,
    void* data,
    uint32_t size, /* unused */
    char** current,
    ext2_dir_entry_t** prev,
    const ext2_dir_entry_t* ent)
{
    EXT2_DECLARE_ERR(err);
    uint32_t rec_len;
    uint32_t offset;
    (void)size;

    /* Compute size of the new directory entry */
    rec_len = sizeof(*ent) - EXT2_PATH_MAX + ent->name_len;
    rec_len = _next_mult(rec_len, 4);

    /* Compute byte offset into current block */
    offset = ((char*)(*current) - (char*)data) % ext2->block_size;

    /* If new entry would overflow the block */
    if (offset + rec_len > ext2->block_size)
    {
        uint32_t rem = ext2->block_size - offset;

        if (!(*prev))
        {
            GOTO(done);
        }

        /* Adjust previous entry to point to next block */
        (*prev)->rec_len += rem;
        (*current) += rem;
    }

    /* Copy this entry into new buffer */
    {
        ext2_dir_entry_t* tmp_ent;

        /* Set pointer to next new entry */
        tmp_ent = (ext2_dir_entry_t*)(*current);

        /* Copy over new entry */
        memset(tmp_ent, 0, rec_len);
        memcpy(tmp_ent, ent, sizeof(*ent) - EXT2_PATH_MAX + ent->name_len);
        tmp_ent->rec_len = rec_len;
        (*prev) = tmp_ent;
        (*current) += rec_len;
    }

    err = EXT2_ERR_NONE;

done:
    return err;
}

static ext2_err_t _index_dir_to_linked_list_dir(
    ext2_t* ext2,
    const char* rm_name, /* may be null */
    ext2_dir_entry_t* new_ent, /* may be null */
    const void* data,
    uint32_t size,
    void** new_data,
    uint32_t* new_size)
{
    EXT2_DECLARE_ERR(err);
    const char* src = (const char*)data;
    const char* src_end = (const char*)data + size;
    char* dest = NULL;

    /* Check parameters */
    if (!ext2_valid(ext2) || !data || !size || !new_data || !new_size)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    /* Linked list directory will be smaller than this */
    *new_size = size;

    /* Allocate a buffer to hold 'linked list' directory */
    if (!(*new_data = calloc(*new_size, 1)))
    {
        GOTO(done);
    }

    /* Set current and end pointers to new buffer */
    dest = (char*)*new_data;

    /* Copy over directory entries (skipping removed entry) */
    {
        ext2_dir_entry_t* prev = NULL;

        while (src < src_end)
        {
            const ext2_dir_entry_t* ent;

            /* Set pointer to current entry */
            ent = (const ext2_dir_entry_t*)src;

            /* Skip blank directory entries */
            if (!ent->name)
            {
                src += ent->rec_len;
                continue;
            }

            /* Skip optional entry to be removed */
            if (rm_name && strncmp(rm_name, ent->name, ent->name_len) == 0)
            {
                src += ent->rec_len;
                continue;
            }

            if ((err = _append_dir_entry(
                ext2,
                *new_data,
                *new_size,
                &dest,
                &prev,
                ent)))
            {
                GOTO(done);
            }

            src += ent->rec_len;
        }

        if (new_ent)
        {
            if ((err = _append_dir_entry(
                ext2,
                *new_data,
                *new_size,
                &dest,
                &prev,
                new_ent)))
            {
                GOTO(done);
            }
        }

        /* Set final entry to point to end of the block */
        if (prev)
        {
            uint32_t offset;
            uint32_t rem;

            /* Compute byte offset into current block */
            offset = (dest - (char*)*new_data) % ext2->block_size;

            /* Compute remaining bytes */
            rem = ext2->block_size - offset;

            /* Set record length of final entry to end of block */
            prev->rec_len += rem;

            /* Advance dest to block boundary */
            dest += rem;
        }

        /* Size down the new blocks size */
        *new_size = (uint32_t)(dest - (char*)*new_data);

        /* Perform a sanity check on the new entries */
        if ((err = _check_dir_entries(
            ext2, *new_data, *new_size)))
        {
            GOTO(done);
        }
    }

    err = EXT2_ERR_NONE;

done:
    return err;
}

static ext2_err_t _add_dir_ent(
    ext2_t* ext2,
    ext2_ino_t dir_ino,
    ext2_inode_t* dir_inode,
    const char* filename,
    ext2_dir_entry_t* new_ent)
{
    EXT2_DECLARE_ERR(err);
    void* blocks = NULL;
    uint32_t blocks_size = 0;
    void* new_blocks = NULL;
    uint32_t new_blocks_size = 0;
    buf_u32_t blknos = BUF_U32_INITIALIZER;

    /* Load the directory file */
    if ((err = ext2_load_file_from_inode(
        ext2,
        dir_inode,
        &blocks,
        &blocks_size)))
    {
        GOTO(done);
    }

    /* Load the block numbers (including the block blocks) */
    if ((err = _load_block_numbers_from_inode(
        ext2,
        dir_inode,
        1, /* include_block_blocks */
        &blknos)))
    {
        GOTO(done);
    }

    /* Find 'filename' within this directory */
    if ((_find_dir_entry(filename, blocks, blocks_size)))
    {
        err = EXT2_ERR_FILE_NOT_FOUND;
        GOTO(done);
    }

    /* Convert from 'indexed' to 'linked list' directory format */
    if ((err = _index_dir_to_linked_list_dir(
        ext2,
        NULL, /* rm_name */
        new_ent, /* new_entry */
        blocks,
        dir_inode->i_size,
        &new_blocks,
        &new_blocks_size)))
    {
        GOTO(done);
    }

    /* Count directory entries before and after */
    {
        uint32_t count;
        uint32_t new_count;

        if ((err = _count_dir_entries(
            ext2,
            blocks,
            blocks_size,
            &count)))
        {
            GOTO(done);
        }

        if ((err = _count_dir_entries(
            ext2,
            new_blocks,
            new_blocks_size,
            &new_count)))
        {
            GOTO(done);
        }

        if (count + 1 != new_count)
        {
            GOTO(done);
        }
    }

    /* Return all directory blocks to the free list */
    if ((err = _put_blocks(ext2, blknos.data, blknos.size)))
    {
        GOTO(done);
    }

    /* Update the directory inode blocks */
    if ((err = _update_inode_data_blocks(
        ext2,
        dir_ino,
        dir_inode,
        new_blocks,
        new_blocks_size,
        1))) /* is_dir */
    {
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:

    if (blocks)
        free(blocks);

    if (new_blocks)
        free(new_blocks);

    buf_u32_release(&blknos);

    return err;
}

ext2_err_t ext2_put(
    ext2_t* ext2,
    const void* data,
    uint32_t size,
    const char* path,
    uint16_t mode)
{
    EXT2_DECLARE_ERR(err);
    char dirname[EXT2_PATH_MAX];
    char filename[EXT2_PATH_MAX];
    ext2_ino_t file_ino;
    ext2_inode_t file_inode;
    ext2_ino_t dir_ino;
    ext2_inode_t dir_inode;
    buf_u32_t blknos = BUF_U32_INITIALIZER;
    ext2_dir_ent_buf_t ent_buf;
    ext2_dir_entry_t* ent = &ent_buf.base;

    /* Check parameters */
    if (!ext2_valid(ext2) || !data || !size || !path)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    /* Reject attempts to copy directories */
    if (mode & EXT2_S_IFDIR)
    {
        GOTO(done);
    }

    /* Split the path */
    if ((err = _split_full_path(path, dirname, filename)))
    {
        GOTO(done);
    }

    /* Read inode for the directory */
    if ((err = ext2_path_to_inode(ext2, dirname, &dir_ino, &dir_inode)))
    {
        GOTO(done);
    }

    /* Read inode for the file (if any) */
    if (!(err = ext2_path_to_inode(ext2, path, &file_ino, &file_inode)))
    {
        /* Disallow overwriting of directory */
        if (!(file_inode.i_mode & EXT2_S_IFDIR))
        {
            GOTO(done);
        }

        /* Delete the file if it exists */
        if ((err = ext2_rm(ext2, path)))
        {
            GOTO(done);
        }
    }

    /* Write the blocks of the file */
    if ((err = _write_data(ext2, data, size, &blknos)))
        GOTO(done);

    /* Create an inode for this new file */
    if ((err = _create_file_inode(
        ext2,
        data,
        size,
        mode,
        blknos.data,
        blknos.size,
        &file_ino)))
    {
        GOTO(done);
    }

    /* Initialize the new directory entry */
    {
        /* ent->inode */
        ent->inode = file_ino;

        /* ent->name_len */
        ent->name_len = (uint32_t)strlen(filename);

        /* ent->file_type */
        ent->file_type = EXT2_FT_REG_FILE;

        /* ent->name[] */
        ent->name[0] = '\0';
        _strncat(ent->name, sizeof(ent->name), filename, EXT2_PATH_MAX-1);

        /* ent->rec_len */
        ent->rec_len =
            _next_mult(sizeof(*ent)- EXT2_PATH_MAX + ent->name_len, 4);
    }

    /* Create new entry for this file in the directory inode */
    if ((err = _add_dir_ent(
        ext2,
        dir_ino,
        &dir_inode,
        filename,
        ent)))
    {
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:

    buf_u32_release(&blknos);

    return err;
}

ext2_err_t ext2_mkdir(ext2_t* ext2, const char* path, uint16_t mode)
{
    EXT2_DECLARE_ERR(err);
    char dirname[EXT2_PATH_MAX];
    char basename[EXT2_PATH_MAX];
    ext2_ino_t dir_ino;
    ext2_inode_t dir_inode;
    ext2_ino_t base_ino;
    ext2_inode_t base_inode;
    ext2_dir_ent_buf_t ent_buf;
    ext2_dir_entry_t* ent = &ent_buf.base;

    /* Check parameters */
    if (!ext2_valid(ext2) || !path || !mode)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    /* Reject is not a directory */
    if (!(mode & EXT2_S_IFDIR))
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        GOTO(done);
    }

    /* Split the path */
    if ((err = _split_full_path(path, dirname, basename)))
    {
        GOTO(done);
    }

    /* Read inode for 'dirname' */
    if ((err = ext2_path_to_inode(
        ext2, dirname, &dir_ino, &dir_inode)))
    {
        GOTO(done);
    }

    /* Fail if the directory already exists */
    if (!(err = ext2_path_to_inode(
        ext2, path, &base_ino, &base_inode)))
    {
        GOTO(done);
    }

    /* Create the directory inode and its one block */
    if ((err = _create_dir_inode_and_block(
        ext2,
        dir_ino,
        mode,
        &base_ino)))
    {
        GOTO(done);
    }

    /* Initialize the new directory entry */
    {
        /* ent->inode */
        ent->inode = base_ino;

        /* ent->name_len */
        ent->name_len = (uint32_t)strlen(basename);

        /* ent->file_type */
        ent->file_type = EXT2_FT_DIR;

        /* ent->name[] */
        ent->name[0] = '\0';
        _strncat(ent->name, sizeof(ent->name), basename, EXT2_PATH_MAX-1);

        /* ent->rec_len */
        ent->rec_len =
            _next_mult(sizeof(*ent) - EXT2_PATH_MAX + ent->name_len, 4);
    }

    /* Include new child directory in parent directory's i_links_count */
    dir_inode.i_links_count++;

    /* Create new entry for this file in the directory inode */
    if ((err = _add_dir_ent(ext2, dir_ino, &dir_inode, basename, ent)))
    {
        GOTO(done);
    }

    err = EXT2_ERR_NONE;

done:
    return err;
}

/*
**==============================================================================
**
** ext2_file_t
**
**==============================================================================
*/

struct _ext2_file
{
    ext2_t* ext2;
    ext2_inode_t inode;
    buf_u32_t blknos;
    uint64_t offset;
    bool eof;
};

ext2_file_t* ext2_open_file(
    ext2_t* ext2,
    const char* path,
    ext2_file_mode_t mode)
{
    ext2_file_t* file = NULL;
    ext2_ino_t ino;
    ext2_inode_t inode;
    buf_u32_t blknos = BUF_U32_INITIALIZER;

    /* Reject null parameters */
    if (!ext2 || !path)
        goto done;

    /* Find the inode for this file */
    if (ext2_path_to_inode(ext2, path, &ino, &inode) != EXT2_ERR_NONE)
        goto done;

    /* Load the block numbers for this inode */
    if (_load_block_numbers_from_inode(
        ext2,
        &inode,
        0, /* include_block_blocks */
        &blknos) != EXT2_ERR_NONE)
    {
        goto done;
    }

    /* Allocate and initialize the file object */
    {
        if (!(file = (ext2_file_t*)calloc(1, sizeof(ext2_file_t))))
            goto done;

        file->ext2 = ext2;
        file->inode = inode;
        file->blknos = blknos;
        file->offset = 0;
    }

done:
    if (!file)
        buf_u32_release(&blknos);

    return file;
}

int64_t ext2_file_read(
    ext2_file_t* file,
    void* data,
    uint64_t size)
{
    int64_t nread = -1;
    uint32_t first;
    uint32_t i;
    uint64_t remaining;
    uint8_t* end = (uint8_t*)data;

    /* Check parameters */
    if (!file || !file->ext2 || !data)
        goto done;

    /* The index of the first block to read: ext2->blknos[first] */
    first = file->offset / file->ext2->block_size;

    /* The number of bytes remaining to be read */
    remaining = size;

    /* Read the data block-by-block */
    for (i = first; i < file->blknos.size && remaining > 0 && !file->eof; i++)
    {
        ext2_block_t block;
        uint32_t offset;

        if (ext2_read_block(
            file->ext2,
            file->blknos.data[i],
            &block) != EXT2_ERR_NONE)
        {
            goto done;
        }

        /* The offset of the data within this block */
        offset = file->offset % file->ext2->block_size;

        /* Copy the minimum of these to the caller's buffer:
         *     remaining
         *     block-bytes-remaining
         *     file-bytes-remaining
         */
        {
            uint64_t copyBytes;

            /* Bytes to copy to user buffer */
            copyBytes = remaining;

            /* Reduce copyBytes to bytes remaining in the block? */
            {
                uint64_t blockBytesRemaining = file->ext2->block_size - offset;

                if (blockBytesRemaining < copyBytes)
                    copyBytes = blockBytesRemaining;
            }

            /* Reduce copyBytes to bytes remaining in the file? */
            {
                uint64_t fileBytesRemaining = file->inode.i_size - file->offset;

                if (fileBytesRemaining < copyBytes)
                {
                    copyBytes = fileBytesRemaining;
                    file->eof = true;
                }
            }

            /* Copy data to user buffer */
            memcpy(end, block.data + offset, copyBytes);
            remaining -= copyBytes;
            end += copyBytes;
            file->offset += copyBytes;
        }
    }

    /* Calculate number of bytes read */
    nread = size - remaining;

done:
    return nread;
}

int64_t ext2_file_write(ext2_file_t* file, const void* data, uint64_t size)
{
    /* Unsupported */
    return -1;
}

int ext2_flush_file(ext2_file_t* file)
{
    int rc = -1;

    /* Check parameters */
    if (!file || !file->ext2)
        goto done;

    /* No-op because ext2_t is always flushed */

    rc = 0;

done:
    return rc;
}

int ext2_file_close(ext2_file_t* file)
{
    int rc = -1;

    /* Check parameters */
    if (!file || !file->ext2)
        goto done;

    /* Release the block numbers buffer */
    buf_u32_release(&file->blknos);

    /* Release the file object */
    free(file);

    rc = 0;

done:
    return rc;
}

int ext2_file_seek(ext2_file_t* file, int64_t offset)
{
    int rc = -1;

    if (!file)
        goto done;

    if (offset > file->inode.i_size)
        goto done;

    file->offset = offset;

    rc = 0;

done:
    return rc;
}

int64_t ext2_file_tell(ext2_file_t* file)
{
    return file ? file->offset : -1;
}

int64_t ext2_file_size(ext2_file_t* file)
{
    return file ? file->inode.i_size : -1;
}

ext2_err_t ext2_get_block_numbers(
    ext2_t* ext2,
    const char* path,
    buf_u32_t* blknos)
{
    EXT2_DECLARE_ERR(err);
    ext2_ino_t ino;
    ext2_inode_t inode;

    /* Reject null parameters */
    if (!ext2 || !path || !blknos)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        goto done;
    }

    /* Find the inode for this file */
    if (ext2_path_to_inode(ext2, path, &ino, &inode) != EXT2_ERR_NONE)
        goto done;

    /* Load the block numbers for this inode */
    if ((err = _load_block_numbers_from_inode(
        ext2,
        &inode,
        1, /* include_block_blocks */
        blknos)))
    {
        goto done;
    }

    err = EXT2_ERR_NONE;

done:

    return err;
}

uint64_t ext2_blkno_to_lba(const ext2_t* ext2, uint32_t blkno)
{
    return block_offset(blkno, ext2->block_size) / BLKDEV_BLKSIZE;
}

ext2_err_t ext2_get_first_blkno(
    const ext2_t* ext2,
    ext2_ino_t ino,
    uint32_t* blkno)
{
    EXT2_DECLARE_ERR(err);
    ext2_inode_t inode;

    /* Initialize output parameter */
    if (blkno)
        *blkno = 0;

    /* Reject null parameters */
    if (!ext2 || !blkno)
    {
        err = EXT2_ERR_INVALID_PARAMETER;
        goto done;
    }

    /* Read the inode into memory */
    if (ext2_read_inode(ext2, ino, &inode) != EXT2_ERR_NONE)
        goto done;

    /* Get the block number from the inode */
    *blkno = inode.i_block[0];

    err = EXT2_ERR_NONE;

done:

    return err;
}
