#ifndef OHOS_EXT2_H
#define OHOS_EXT2_H

#include "types.h"

#define EXT2_MAGIC 0xEF53

#define EXT2_S_IFREG 0x8000
#define EXT2_S_IFDIR 0x4000
#define EXT2_S_IFCHR 0x2000
#define EXT2_S_IFBLK 0x6000
#define EXT2_S_IFIFO 0x1000
#define EXT2_S_IFLNK 0xA000
#define EXT2_S_IFSOCK 0xC000

#define EXT2_NAME_LEN 255

struct ext2_superblock {
    u32 s_inodes_count;
    u32 s_blocks_count;
    u32 s_r_blocks_count;
    u32 s_free_blocks_count;
    u32 s_free_inodes_count;
    u32 s_first_data_block;
    u32 s_log_block_size;
    u32 s_log_frag_size;
    u32 s_blocks_per_group;
    u32 s_frags_per_group;
    u32 s_inodes_per_group;
    u32 s_mtime;
    u32 s_wtime;
    u16 s_mnt_count;
    u16 s_max_mnt_count;
    u16 s_magic;
    u16 s_state;
    u16 s_errors;
    u16 s_minor_rev_level;
    u32 s_lastcheck;
    u32 s_checkinterval;
    u32 s_creator_os;
    u32 s_rev_level;
    u16 s_def_resuid;
    u16 s_def_resgid;
    u32 s_first_ino;
    u16 s_inode_size;
    u16 s_block_group_nr;
    u32 s_feature_compat;
    u32 s_feature_incompat;
    u32 s_feature_ro_compat;
    u8 s_uuid[16];
    char s_volume_name[16];
    char s_last_mounted[64];
    u32 s_algo_bitmap;
} PACKED;

struct ext2_block_group_desc {
    u32 bg_block_bitmap;
    u32 bg_inode_bitmap;
    u32 bg_inode_table;
    u16 bg_free_blocks_count;
    u16 bg_free_inodes_count;
    u16 bg_used_dirs_count;
    u16 bg_pad;
    u32 bg_reserved[3];
} PACKED;

struct ext2_inode {
    u16 i_mode;
    u16 i_uid;
    u32 i_size;
    u32 i_atime;
    u32 i_ctime;
    u32 i_mtime;
    u32 i_dtime;
    u16 i_gid;
    u16 i_links_count;
    u32 i_blocks;
    u32 i_flags;
    u32 i_osd1;
    u32 i_block[15];
    u32 i_generation;
    u32 i_file_acl;
    u32 i_dir_acl;
    u32 i_faddr;
    u8 i_osd2[12];
} PACKED;

struct ext2_dir_entry {
    u32 inode;
    u16 rec_len;
    u8 name_len;
    u8 file_type;
    char name[EXT2_NAME_LEN];
} PACKED;

struct ext2_fs {
    u32 blkdev_id;
    u32 partition_start;
    struct ext2_superblock sb;
    u32 block_size;
    u32 sectors_per_block;
    u32 inodes_per_block;
    u32 groups_count;
    struct ext2_block_group_desc *bgdt;
    bool mounted;
};

struct ext2_fs;

typedef struct {
    struct ext2_fs *fs;
    u32 inode_num;
    struct ext2_inode inode;
    u32 offset;
    u8 *block_buffer;
    u32 current_block;
} ext2_file_t;

bool ext2_mount(u32 blkdev_id, u32 partition_start, struct ext2_fs *fs);
void ext2_unmount(struct ext2_fs *fs);
bool ext2_is_valid(u32 blkdev_id, u32 partition_start);

u32 ext2_find_inode(struct ext2_fs *fs, const char *path);
bool ext2_read_inode(struct ext2_fs *fs, u32 inode_num, struct ext2_inode *inode);
bool ext2_read_block(struct ext2_fs *fs, u32 block_num, void *buffer);
bool ext2_read_inode_block(struct ext2_fs *fs, struct ext2_inode *inode, u32 block_idx, void *buffer);

bool ext2_opendir(struct ext2_fs *fs, u32 inode_num, ext2_file_t *dir);
bool ext2_readdir(ext2_file_t *dir, struct ext2_dir_entry *entry, char *name_buf);
void ext2_closedir(ext2_file_t *dir);

bool ext2_open(struct ext2_fs *fs, u32 inode_num, ext2_file_t *file);
ssize_t ext2_read(ext2_file_t *file, void *buffer, size_t size);
void ext2_close(ext2_file_t *file);

u32 ext2_file_size(struct ext2_inode *inode);
bool ext2_is_dir(struct ext2_inode *inode);
bool ext2_is_reg(struct ext2_inode *inode);

#endif
