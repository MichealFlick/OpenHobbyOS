#ifndef OHOS_VFS_H
#define OHOS_VFS_H

#include "ext2.h"
#include "initrd.h"
#include "types.h"

#define VFS_NAME_MAX 64
#define VFS_PATH_MAX 256

/* Filesystem types */
enum vfs_type {
    VFS_TYPE_NONE = 0,
    VFS_TYPE_RAMFS,      /* initrd-backed ramfs */
    VFS_TYPE_EXT2,       /* ext2 filesystem */
};

typedef struct vfs_node vfs_node_t;

typedef struct vfs_ramfile {
    void *backing_allocation;
    char *data;
    u32 size;
    u32 capacity;
    u32 links;
} vfs_ramfile_t;

typedef struct {
    u64 inode;
    u32 mode;
    u32 size;
    u32 block_size;
    u64 blocks;
    u32 nlink;
    bool is_dir;
} vfs_stat_t;

/* VFS initialization - pass initrd handle, may also mount ext2 */
void vfs_init(void);
bool vfs_init_from_initrd(void);
bool vfs_init_from_ext2(u32 blkdev_id, u32 partition_start);

bool vfs_ready(void);
const vfs_node_t *vfs_root(void);
const vfs_node_t *vfs_resolve(const vfs_node_t *cwd, const char *path);
int vfs_mkdir(const char *path);
const vfs_node_t *vfs_parent(const vfs_node_t *node);
const char *vfs_name(const vfs_node_t *node);
const char *vfs_path(const vfs_node_t *node);
u64 vfs_inode(const vfs_node_t *node);
bool vfs_is_dir(const vfs_node_t *node);
bool vfs_is_file(const vfs_node_t *node);

/* Filesystem type detection */
enum vfs_type vfs_node_type(const vfs_node_t *node);
bool vfs_node_is_ext2(const vfs_node_t *node);

/* For backward compat - returns initrd file if ramfs type */
const initrd_file_t *vfs_backing_file(const vfs_node_t *node);
u32 vfs_file_size(const vfs_node_t *node);
ssize_t vfs_read(const vfs_node_t *node, u32 offset, void *buffer, size_t length);
bool vfs_stat_node(const vfs_node_t *node, vfs_stat_t *stat);
u32 vfs_child_count(const vfs_node_t *node);
const vfs_node_t *vfs_child_at(const vfs_node_t *node, u32 index);

bool vfs_node_is_ramfile(const vfs_node_t *node);
vfs_ramfile_t *vfs_node_ramfile(const vfs_node_t *node);
const vfs_node_t *vfs_create_ramfile(const char *path);
ssize_t vfs_write_ramfile(const vfs_node_t *node, u32 offset, const void *buffer, size_t length);
int vfs_unlink_ramfile(const char *path);
int vfs_link_ramfile(const char *oldpath, const char *newpath);
int vfs_rename_ramfile(const char *oldpath, const char *newpath);

/* Ext2 specific access */
struct ext2_fs *vfs_ext2_fs(void);
bool vfs_has_ext2_root(void);

#endif
