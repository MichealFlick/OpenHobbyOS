#ifndef OHOS_VFS_H
#define OHOS_VFS_H

#include "initrd.h"
#include "types.h"

#define VFS_NAME_MAX 64
#define VFS_PATH_MAX 256

typedef struct vfs_node vfs_node_t;

typedef struct {
    u64 inode;
    u32 mode;
    u32 size;
    u32 block_size;
    u64 blocks;
    bool is_dir;
} vfs_stat_t;

void vfs_init(void);
bool vfs_ready(void);
const vfs_node_t *vfs_root(void);
const vfs_node_t *vfs_resolve(const vfs_node_t *cwd, const char *path);
const vfs_node_t *vfs_parent(const vfs_node_t *node);
const char *vfs_name(const vfs_node_t *node);
const char *vfs_path(const vfs_node_t *node);
u64 vfs_inode(const vfs_node_t *node);
bool vfs_is_dir(const vfs_node_t *node);
bool vfs_is_file(const vfs_node_t *node);
const initrd_file_t *vfs_backing_file(const vfs_node_t *node);
u32 vfs_file_size(const vfs_node_t *node);
ssize_t vfs_read(const vfs_node_t *node, u32 offset, void *buffer, size_t length);
bool vfs_stat_node(const vfs_node_t *node, vfs_stat_t *stat);
u32 vfs_child_count(const vfs_node_t *node);
const vfs_node_t *vfs_child_at(const vfs_node_t *node, u32 index);

#endif
