#include "ext2.h"

#include "blkdev.h"
#include "console.h"
#include "memory.h"
#include "panic.h"
#include "string.h"

#define EXT2_BLOCK_SIZE(fs) (1024 << (fs)->sb.s_log_block_size)
#define EXT2_INODES_PER_BLOCK(fs) ((fs)->block_size / (fs)->sb.s_inode_size)
#define EXT2_SECTOR_FROM_BLOCK(fs, block) ((fs)->partition_start + (block) * (fs)->sectors_per_block)

bool ext2_read_block(struct ext2_fs *fs, u32 block_num, void *buffer) {
    u32 lba = EXT2_SECTOR_FROM_BLOCK(fs, block_num);
    u32 sectors = fs->sectors_per_block;
    
    if (blkdev_read(fs->blkdev_id, lba, sectors, buffer) != 0) {
        return false;
    }
    return true;
}

bool ext2_is_valid(u32 blkdev_id, u32 partition_start) {
    if (!blkdev_present(blkdev_id)) {
        return false;
    }
    
    static struct ext2_superblock sb;
    u32 sb_offset = partition_start + 2;
    
    if (blkdev_read(blkdev_id, sb_offset, 2, &sb) != 0) {
        return false;
    }
    
    return sb.s_magic == EXT2_MAGIC;
}

bool ext2_mount(u32 blkdev_id, u32 partition_start, struct ext2_fs *fs) {
    memset(fs, 0, sizeof(struct ext2_fs));
    
    fs->blkdev_id = blkdev_id;
    fs->partition_start = partition_start;
    
    u32 sb_offset = partition_start + 2;
    if (blkdev_read(blkdev_id, sb_offset, 2, &fs->sb) != 0) {
        return false;
    }
    
    if (fs->sb.s_magic != EXT2_MAGIC) {
        return false;
    }
    
    fs->block_size = EXT2_BLOCK_SIZE(fs);
    fs->sectors_per_block = fs->block_size / BLKDEV_SECTOR_SIZE;
    fs->inodes_per_block = EXT2_INODES_PER_BLOCK(fs);
    
    fs->groups_count = (fs->sb.s_blocks_count + fs->sb.s_blocks_per_group - 1) / fs->sb.s_blocks_per_group;
    
    u32 bgdt_block = (fs->sb.s_first_data_block == 0) ? 2 : 1;
    if (fs->block_size == 1024) {
        bgdt_block = 2;
    }
    
    u32 bgdt_size = fs->groups_count * sizeof(struct ext2_block_group_desc);
    u32 bgdt_blocks = (bgdt_size + fs->block_size - 1) / fs->block_size;
    
    fs->bgdt = kmalloc(bgdt_blocks * fs->block_size);
    if (!fs->bgdt) {
        return false;
    }
    
    for (u32 i = 0; i < bgdt_blocks; i++) {
        if (!ext2_read_block(fs, bgdt_block + i, (u8 *)fs->bgdt + i * fs->block_size)) {
            kfree(fs->bgdt);
            return false;
        }
    }
    
    fs->mounted = true;
    
    console_printf("[ext2] mounted: %u blocks, %u inodes, %u block size\n",
                   fs->sb.s_blocks_count, fs->sb.s_inodes_count, fs->block_size);
    
    return true;
}

void ext2_unmount(struct ext2_fs *fs) {
    if (fs->bgdt) {
        kfree(fs->bgdt);
        fs->bgdt = NULL;
    }
    fs->mounted = false;
}

bool ext2_read_inode(struct ext2_fs *fs, u32 inode_num, struct ext2_inode *inode) {
    if (inode_num == 0 || inode_num > fs->sb.s_inodes_count) {
        return false;
    }
    
    u32 group = (inode_num - 1) / fs->sb.s_inodes_per_group;
    u32 index = (inode_num - 1) % fs->sb.s_inodes_per_group;
    
    if (group >= fs->groups_count) {
        return false;
    }
    
    u32 inode_table_block = fs->bgdt[group].bg_inode_table;
    u32 block_offset = index / fs->inodes_per_block;
    u32 inode_offset = index % fs->inodes_per_block;
    
    u8 *block_buffer = kmalloc(fs->block_size);
    if (!block_buffer) {
        return false;
    }
    
    if (!ext2_read_block(fs, inode_table_block + block_offset, block_buffer)) {
        kfree(block_buffer);
        return false;
    }
    
    struct ext2_inode *inode_table = (struct ext2_inode *)block_buffer;
    memcpy(inode, &inode_table[inode_offset], sizeof(struct ext2_inode));
    
    kfree(block_buffer);
    return true;
}

bool ext2_read_inode_block(struct ext2_fs *fs, struct ext2_inode *inode, u32 block_idx, void *buffer) {
    u32 block_num = 0;
    
    if (block_idx < 12) {
        block_num = inode->i_block[block_idx];
    } else if (block_idx < 12 + 256) {
        u32 indirect_idx = block_idx - 12;
        u8 *indirect_buf = kmalloc(fs->block_size);
        if (!indirect_buf) return false;
        
        if (!ext2_read_block(fs, inode->i_block[12], indirect_buf)) {
            kfree(indirect_buf);
            return false;
        }
        
        u32 *indirect = (u32 *)indirect_buf;
        block_num = indirect[indirect_idx];
        kfree(indirect_buf);
    } else {
        return false;
    }
    
    if (block_num == 0) {
        memset(buffer, 0, fs->block_size);
        return true;
    }
    
    return ext2_read_block(fs, block_num, buffer);
}

static u32 ext2_dir_lookup(struct ext2_fs *fs, struct ext2_inode *dir_inode, const char *name) {
    u8 *block_buffer = kmalloc(fs->block_size);
    if (!block_buffer) {
        return 0;
    }
    
    u32 num_blocks = (dir_inode->i_size + fs->block_size - 1) / fs->block_size;
    
    for (u32 i = 0; i < num_blocks; i++) {
        if (!ext2_read_inode_block(fs, dir_inode, i, block_buffer)) {
            continue;
        }
        
        u32 offset = 0;
        while (offset < fs->block_size) {
            struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(block_buffer + offset);
            
            if (entry->inode == 0) {
                offset += entry->rec_len;
                continue;
            }
            
            char entry_name[EXT2_NAME_LEN + 1];
            u32 name_len = entry->name_len;
            if (name_len > EXT2_NAME_LEN) {
                name_len = EXT2_NAME_LEN;
            }
            memcpy(entry_name, entry->name, name_len);
            entry_name[name_len] = '\0';
            
            if (strcmp(entry_name, name) == 0) {
                u32 result = entry->inode;
                kfree(block_buffer);
                return result;
            }
            
            if (entry->rec_len == 0) {
                break;
            }
            offset += entry->rec_len;
        }
    }
    
    kfree(block_buffer);
    return 0;
}

u32 ext2_find_inode(struct ext2_fs *fs, const char *path) {
    if (!fs->mounted) {
        return 0;
    }
    
    if (path[0] != '/') {
        return 0;
    }
    
    if (path[1] == '\0') {
        return 2;
    }
    
    u32 current_inode = 2;
    char path_copy[256];
    strncpy(path_copy, path, 255);
    path_copy[255] = '\0';
    
    char *token = strtok(path_copy + 1, "/");
    struct ext2_inode inode;
    
    while (token != NULL) {
        if (!ext2_read_inode(fs, current_inode, &inode)) {
            return 0;
        }
        
        if (!ext2_is_dir(&inode)) {
            return 0;
        }
        
        current_inode = ext2_dir_lookup(fs, &inode, token);
        if (current_inode == 0) {
            return 0;
        }
        
        token = strtok(NULL, "/");
    }
    
    return current_inode;
}

bool ext2_opendir(struct ext2_fs *fs, u32 inode_num, ext2_file_t *dir) {
    if (!ext2_read_inode(fs, inode_num, &dir->inode)) {
        return false;
    }
    
    if (!ext2_is_dir(&dir->inode)) {
        return false;
    }
    
    dir->fs = fs;
    dir->inode_num = inode_num;
    dir->offset = 0;
    dir->current_block = 0xFFFFFFFF;
    dir->block_buffer = kmalloc(fs->block_size);
    
    return dir->block_buffer != NULL;
}

bool ext2_readdir(ext2_file_t *dir, struct ext2_dir_entry *entry, char *name_buf) {
    struct ext2_fs *fs = dir->fs;
    
    if (!fs || !fs->mounted) {
        return false;
    }
    
    u32 block_size = fs->block_size;
    u32 num_blocks = (dir->inode.i_size + block_size - 1) / block_size;
    
    while (dir->offset < dir->inode.i_size) {
        u32 block_idx = dir->offset / block_size;
        u32 block_offset = dir->offset % block_size;
        
        if (block_idx >= num_blocks) {
            return false;
        }
        
        if (block_idx != dir->current_block) {
            if (!ext2_read_inode_block(fs, &dir->inode, block_idx, dir->block_buffer)) {
                return false;
            }
            dir->current_block = block_idx;
        }
        
        struct ext2_dir_entry *de = (struct ext2_dir_entry *)(dir->block_buffer + block_offset);
        
        if (de->inode != 0) {
            memcpy(entry, de, sizeof(struct ext2_dir_entry));
            
            u32 name_len = de->name_len;
            if (name_len > EXT2_NAME_LEN) {
                name_len = EXT2_NAME_LEN;
            }
            memcpy(name_buf, de->name, name_len);
            name_buf[name_len] = '\0';
            
            dir->offset += de->rec_len;
            return true;
        }
        
        if (de->rec_len == 0) {
            return false;
        }
        
        dir->offset += de->rec_len;
    }
    
    return false;
}

void ext2_closedir(ext2_file_t *dir) {
    if (dir->block_buffer) {
        kfree(dir->block_buffer);
        dir->block_buffer = NULL;
    }
}

bool ext2_open(struct ext2_fs *fs, u32 inode_num, ext2_file_t *file) {
    if (!ext2_read_inode(fs, inode_num, &file->inode)) {
        return false;
    }
    
    file->fs = fs;
    file->inode_num = inode_num;
    file->offset = 0;
    file->current_block = 0xFFFFFFFF;
    file->block_buffer = kmalloc(fs->block_size);
    
    return file->block_buffer != NULL;
}

ssize_t ext2_read(ext2_file_t *file, void *buffer, size_t size) {
    struct ext2_fs *fs = file->fs;
    
    if (!fs || !fs->mounted) {
        return -1;
    }
    
    u32 file_size = file->inode.i_size;
    if (file->offset >= file_size) {
        return 0;
    }
    
    if (file->offset + size > file_size) {
        size = file_size - file->offset;
    }
    
    u8 *buf = buffer;
    size_t total_read = 0;
    u32 block_size = fs->block_size;
    
    while (size > 0 && file->offset < file_size) {
        u32 block_idx = file->offset / block_size;
        u32 block_offset = file->offset % block_size;
        u32 to_read = block_size - block_offset;
        
        if (to_read > size) {
            to_read = size;
        }
        
        if (file->offset + to_read > file_size) {
            to_read = file_size - file->offset;
        }
        
        if (block_idx != file->current_block) {
            if (!ext2_read_inode_block(fs, &file->inode, block_idx, file->block_buffer)) {
                break;
            }
            file->current_block = block_idx;
        }
        
        memcpy(buf, file->block_buffer + block_offset, to_read);
        
        buf += to_read;
        file->offset += to_read;
        total_read += to_read;
        size -= to_read;
    }
    
    return total_read;
}

void ext2_close(ext2_file_t *file) {
    if (file->block_buffer) {
        kfree(file->block_buffer);
        file->block_buffer = NULL;
    }
}

u32 ext2_file_size(struct ext2_inode *inode) {
    return inode->i_size;
}

bool ext2_is_dir(struct ext2_inode *inode) {
    return (inode->i_mode & 0xF000) == EXT2_S_IFDIR;
}

bool ext2_is_reg(struct ext2_inode *inode) {
    return (inode->i_mode & 0xF000) == EXT2_S_IFREG;
}
