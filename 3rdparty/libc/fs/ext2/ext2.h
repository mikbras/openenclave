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

#ifndef _EXT2_H
#define _EXT2_H

#include "config.h"
#include "blkdev.h"
#include "buf.h"

/*
**==============================================================================
**
** basic types:
**
**==============================================================================
*/

typedef unsigned int ext2_ino_t;
typedef unsigned int ext2_off_t;

/*
**==============================================================================
**
** errors:
**
**==============================================================================
*/

typedef enum _ext2_err
{
    EXT2_ERR_NONE,
    EXT2_ERR_FAILED,
    EXT2_ERR_INVALID_PARAMETER,
    EXT2_ERR_FILE_NOT_FOUND,
    EXT2_ERR_BAD_MAGIC,
    EXT2_ERR_UNSUPPORTED,
    EXT2_ERR_OUT_OF_MEMORY,
    EXT2_ERR_FAILED_TO_READ_SUPERBLOCK,
    EXT2_ERR_FAILED_TO_READ_GROUPS,
    EXT2_ERR_FAILED_TO_READ_INODE,
    EXT2_ERR_UNSUPPORTED_REVISION,
    EXT2_ERR_OPEN_FAILED,
    EXT2_ERR_BUFFER_OVERFLOW,
    EXT2_ERR_SEEK_FAILED,
    EXT2_ERR_READ_FAILED,
    EXT2_ERR_WRITE_FAILED,
    EXT2_ERR_UNEXPECTED,
    EXT2_ERR_SANITY_CHECK_FAILED,
    EXT2_ERR_BAD_BLKNO,
    EXT2_ERR_BAD_INO,
    EXT2_ERR_BAD_GRPNO,
    EXT2_ERR_BAD_MULTIPLE,
    EXT2_ERR_EXTRANEOUS_DATA,
    EXT2_ERR_BAD_SIZE,
    EXT2_ERR_PATH_TOO_LONG,
}
ext2_err_t;

const char* ext2_err_str(ext2_err_t err);

/*
**==============================================================================
**
** structure typedefs:
**
**==============================================================================
*/

typedef struct _ext2 ext2_t;
typedef struct _ext2_block ext2_block_t;
typedef struct _ext2_super_block ext2_super_block_t;
typedef struct _ext2_group_desc ext2_group_desc_t;
typedef struct _ext2_inode ext2_inode_t;
typedef struct _ext2_dir_entry ext2_dir_entry_t;
typedef struct _ext2_dir ext2_dir_t;

/*
**==============================================================================
**
** blocks:
**
**==============================================================================
*/

#define EXT2_MAX_BLOCK_SIZE (8 * 1024)

struct _ext2_block
{
    uint8_t data[EXT2_MAX_BLOCK_SIZE];
    uint32_t size;
};

ext2_err_t ext2_read_block(
    const ext2_t* ext2,
    uint32_t blkno,
    ext2_block_t* block);

ext2_err_t ext2_write_block(
    const ext2_t* ext2,
    uint32_t blkno,
    const ext2_block_t* block);

/*
**==============================================================================
**
** super block:
**
**==============================================================================
*/

/* Offset of super block from start of file system */
#define EXT2_BASE_OFFSET 1024

#define EXT2_S_MAGIC 0xEF53
#define EXT2_GOOD_OLD_REV 0 /* Revision 0 ext2_t */
#define EXT2_DYNAMIC_REV 1 /* Revision 1 ext2_t */

struct _ext2_super_block
{
    /* General */
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;

    /* DYNAMIC_REV Specific */
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t s_uuid[16];
    uint8_t s_volume_name[16];
    uint8_t s_last_mounted[64];
    uint32_t s_algo_bitmap;

    /* Performance Hints */
    uint8_t s_prealloc_blocks;
    uint8_t s_prealloc_dir_blocks;
    uint16_t __alignment;

    /* Journaling Support */
    uint8_t s_journal_uuid[16];
    uint32_t s_journal_inum;
    uint32_t s_journal_dev;
    uint32_t s_last_orphan;

    /* Directory Indexing Support */
    uint32_t s_hash_seed[4];
    uint8_t s_def_hash_version;
    uint8_t padding[3];

    /* Other options */
    uint32_t s_default_mount_options;
    uint32_t s_first_meta_bg;
    uint8_t __unused[760];
};

void ext2_dump_super_block(const ext2_super_block_t* sb);

/*
**==============================================================================
**
** groups:
**
**==============================================================================
*/

struct _ext2_group_desc
{
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;
    uint8_t bg_reserved[12];
};

/*
**==============================================================================
**
** inodes:
**
**==============================================================================
*/

#define EXT2_BAD_INO 1
#define EXT2_ROOT_INO 2
#define EXT2_ACL_IDX_INO 3
#define EXT2_ACL_DATA_INO 4
#define EXT2_BOOT_LOADER_INO 5
#define EXT2_UNDEL_DIR_INO 6
#define EXT2_FIRST_INO 11

#define EXT2_SINGLE_INDIRECT_BLOCK 12
#define EXT2_DOUBLE_INDIRECT_BLOCK 13
#define EXT2_TRIPLE_INDIRECT_BLOCK 14

#define EXT2_S_IFSOCK 0xC000
#define EXT2_S_IFLNK 0xA000
#define EXT2_S_IFREG 0x8000
#define EXT2_S_IFBLK 0x6000
#define EXT2_S_IFDIR 0x4000
#define EXT2_S_IFCHR 0x2000
#define EXT2_S_IFIFO 0x1000

#define EXT2_S_ISUID 0x0800
#define EXT2_S_ISGID 0x0400
#define EXT2_S_ISVTX 0x0200

#define EXT2_S_IRUSR 0x0100
#define EXT2_S_IWUSR 0x0080
#define EXT2_S_IXUSR 0x0040
#define EXT2_S_IRGRP 0x0020
#define EXT2_S_IWGRP 0x0010
#define EXT2_S_IXGRP 0x0008
#define EXT2_S_IROTH 0x0004
#define EXT2_S_IWOTH 0x0002
#define EXT2_S_IXOTH 0x0001

#define EXT2_SECRM_FL 0x00000001
#define EXT2_UNRM_FL 0x00000002
#define EXT2_COMPR_FL 0x00000004
#define EXT2_SYNC_FL 0x00000008
#define EXT2_IMMUTABLE_FL 0x00000010
#define EXT2_APPEND_FL 0x00000020
#define EXT2_NODUMP_FL 0x00000040
#define EXT2_NOATIME_FL 0x00000080
#define EXT2_DIRTY_FL 0x00000100
#define EXT2_COMPRBLK_FL 0x00000200
#define EXT2_NOCOMPR_FL 0x00000400
#define EXT2_ECOMPR_FL 0x00000800
#define EXT2_BTREE_FL 0x00001000
#define EXT2_INDEX_FL 0x00001000
#define EXT2_IMAGIC_FL 0x00002000
#define EXT3_JOURNAL_DATA_FL 0x00004000
#define EXT2_RESERVED_FL 0x80000000

struct _ext2_inode
{
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks;
    uint32_t i_flags;
    uint32_t i_osd1;
    /*
       0:11 -- direct block numbers
       12 -- indirect block number
       13 -- double-indirect block number
       14 -- triple-indirect block number
    */
    uint32_t i_block[15];
    uint32_t i_generation;
    uint32_t i_file_acl;
    uint32_t i_dir_acl;
    uint32_t i_faddr;
    uint8_t i_osd2[12];
    uint8_t dummy[128]; /* sometimes the inode is bigger */
};

void ext2_dump_inode(const ext2_t* ext2, const ext2_inode_t* inode);

ext2_err_t ext2_read_inode(
    const ext2_t* ext2,
    ext2_ino_t ino,
    ext2_inode_t* inode);

ext2_err_t ext2_path_to_ino(
    const ext2_t* ext2,
    const char* path,
    uint32_t* ino);

ext2_err_t ext2_path_to_inode(
    const ext2_t* ext2,
    const char* path,
    ext2_ino_t* ino,
    ext2_inode_t* inode);

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
    ext2_block_t* block);

ext2_err_t ext2_write_block_bitmap(
    const ext2_t* ext2,
    uint32_t group_index,
    const ext2_block_t* block);

ext2_err_t ext2_read_inode_bitmap(
    const ext2_t* ext2,
    uint32_t group_index,
    ext2_block_t* block);

/*
**==============================================================================
**
** directories:
**
**==============================================================================
*/

#define EXT2_PATH_MAX 256

#define EXT2_FT_UNKNOWN 0
#define EXT2_FT_REG_FILE 1
#define EXT2_FT_DIR 2
#define EXT2_FT_CHRDEV 3
#define EXT2_FT_BLKDEV 4
#define EXT2_FT_FIFO 5
#define EXT2_FT_SOCK 6
#define EXT2_FT_SYMLINK 7

struct _ext2_dir_entry
{
    uint32_t inode;
    uint16_t rec_len;
    uint8_t name_len;
    uint8_t file_type;
    char name[EXT2_PATH_MAX];
};

#define EXT2_DX_HASH_LEGACY 0
#define EXT2_DX_HASH_HALF_MD4 1
#define EXT2_DX_HASH_TEA 2

#define EXT2_DT_UNKNOWN 0
#define EXT2_DT_FIFO 1
#define EXT2_DT_CHR 2
#define EXT2_DT_DIR 4
#define EXT2_DT_BLK 6
#define EXT2_DT_REG 8
#define EXT2_DT_LNK 10
#define EXT2_DT_SOCK 12
#define EXT2_DT_WHT 14

typedef struct ext2_dir_ent
{
   ext2_ino_t d_ino;
   ext2_off_t d_off;
   uint16_t d_reclen;
   uint8_t d_type;
   char d_name[EXT2_PATH_MAX];
}
ext2_dir_ent_t;

ext2_dir_t* ext2_open_dir(const ext2_t* ext2, const char* name);

ext2_dir_t* ext2_open_dir_ino(const ext2_t* ext2, ext2_ino_t ino);

ext2_dir_ent_t* ext2_read_dir(ext2_dir_t* dir);

ext2_err_t ext2_close_dir(ext2_dir_t* dir);

ext2_err_t ext2_list_dir_inode(
    const ext2_t* ext2,
    uint32_t global_ino,
    ext2_dir_ent_t** entries,
    uint32_t* num_entries);

/*
**==============================================================================
**
** files:
**
**==============================================================================
*/

ext2_err_t ext2_load_file_from_inode(
    const ext2_t* ext2,
    const ext2_inode_t* inode,
    void** data,
    uint32_t* size);

ext2_err_t ext2_load_file_from_path(
    const ext2_t* ext2,
    const char* path,
    void** data,
    uint32_t* size);

/*
**==============================================================================
**
** ext2_t:
**
**==============================================================================
*/

struct _ext2
{
    blkdev_t* dev;
    ext2_super_block_t sb;
    uint32_t block_size;
    uint32_t group_count;
    ext2_group_desc_t* groups;
    ext2_inode_t root_inode;
};

EXT2_INLINE bool ext2_valid(const ext2_t* ext2)
{
    return ext2 != NULL && ext2->sb.s_magic == EXT2_S_MAGIC;
}

ext2_err_t ext2_new(blkdev_t* dev, ext2_t** ext2);

void ext2_delete(ext2_t* ext2);

ext2_err_t ext2_dump(const ext2_t* ext2);

ext2_err_t ext2_check(const ext2_t* ext2);

ext2_err_t ext2_trunc(ext2_t* ext2, const char* path);

ext2_err_t ext2_update(
    ext2_t* ext2,
    const void* data,
    uint32_t size,
    const char* path);

ext2_err_t ext2_rm(ext2_t* ext2, const char* path);

ext2_err_t ext2_put(
    ext2_t* ext2,
    const void* data,
    uint32_t size,
    const char* path,
    uint16_t mode);

ext2_err_t ext2_mkdir(ext2_t* ext2, const char* path, uint16_t mode);

/*
**==============================================================================
**
** ext2_file_t:
**
**==============================================================================
*/

typedef struct _ext2_file ext2_file_t;

typedef enum _ext2_file_mode
{
    EXT2_FILE_RDWR,
    EXT2_FILE_RDONLY,
    EXT2_FILE_WRONLY
}
ext2_file_mode_t;

ext2_file_t* ext2_open_file(
    ext2_t* ext2,
    const char* path,
    ext2_file_mode_t mode);

int64_t ext2_file_read(ext2_file_t* file, void* data, uint64_t size);

int64_t ext2_file_write(ext2_file_t* file, const void* data, uint64_t size);

int ext2_file_seek(ext2_file_t* file, int64_t offset);

int64_t ext2_file_tell(ext2_file_t* file);

int64_t ext2_file_size(ext2_file_t* file);

int ext2_flush_file(ext2_file_t* file);

int ext2_file_close(ext2_file_t* file);

ext2_err_t ext2_get_block_numbers(
    ext2_t* ext2,
    const char* path,
    buf_u32_t* blknos);

uint64_t ext2_blkno_to_lba(const ext2_t* ext2, uint32_t blkno);

ext2_err_t ext2_get_first_blkno(
    const ext2_t* ext2,
    ext2_ino_t ino,
    uint32_t* blkno);

#endif /* _EXT2_H */
