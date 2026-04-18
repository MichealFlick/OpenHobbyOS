#include "vfs.h"

#include "abi/linux.h"
#include "format.h"
#include "initrd.h"
#include "memory.h"
#include "panic.h"
#include "pit.h"
#include "string.h"

struct vfs_node {
    u64 inode;
    char name[VFS_NAME_MAX];
    char path[VFS_PATH_MAX];
    bool is_dir;
    u32 virtual_id;
    const initrd_file_t *file;
    struct vfs_node *parent;
    struct vfs_node *sibling;
    struct vfs_node *child;
};

static struct vfs_node *root_node;
static bool mounted;
static u64 next_inode = 1;

enum {
    VFS_VIRTUAL_NONE = 0,
    VFS_VIRTUAL_PROC_UPTIME,
    VFS_VIRTUAL_PROC_LOADAVG,
    VFS_VIRTUAL_PROC_MEMINFO,
    VFS_VIRTUAL_PROC_CPUINFO,
    VFS_VIRTUAL_PROC_MOUNTS,
    VFS_VIRTUAL_PROC_ROUTE,
    VFS_VIRTUAL_DEV_NULL,
    VFS_VIRTUAL_DEV_TTY,
};

static bool path_segment(const char **cursor, char *segment, size_t segment_size) {
    size_t used = 0;

    while (**cursor == '/') {
        (*cursor)++;
    }
    if (**cursor == '\0') {
        return false;
    }

    while (**cursor && **cursor != '/') {
        if (used + 1 >= segment_size) {
            panic("VFS segment is too long");
        }
        segment[used++] = **cursor;
        (*cursor)++;
    }
    segment[used] = '\0';
    return true;
}

static bool looks_executable(const initrd_file_t *file) {
    return file && file->size >= 4 &&
           file->data[0] == 0x7F &&
           file->data[1] == 'E' &&
           file->data[2] == 'L' &&
           file->data[3] == 'F';
}

static size_t append_text(char *buffer, size_t size, size_t used, const char *text) {
    while (used + 1 < size && *text) {
        buffer[used++] = *text++;
    }
    if (size != 0) {
        buffer[used < size ? used : size - 1] = '\0';
    }
    return used;
}

static size_t append_u32(char *buffer, size_t size, size_t used, u32 value) {
    char digits[16];
    size_t count = 0;

    if (value == 0) {
        return append_text(buffer, size, used, "0");
    }

    while (value && count < sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    while (count != 0 && used + 1 < size) {
        buffer[used++] = digits[--count];
    }
    if (size != 0) {
        buffer[used < size ? used : size - 1] = '\0';
    }
    return used;
}

static size_t format_virtual_file(u32 virtual_id, char *buffer, size_t size) {
    memory_stats_t stats = memory_stats();
    u32 ticks = pit_ticks();
    u32 hz = pit_frequency() ? pit_frequency() : 100u;
    u32 whole = ticks / hz;
    u32 fract = ((ticks % hz) * 100u) / hz;
    size_t used = 0;

    if (size == 0) {
        return 0;
    }

    buffer[0] = '\0';

    switch (virtual_id) {
        case VFS_VIRTUAL_PROC_UPTIME:
            used = append_u32(buffer, size, used, whole);
            used = append_text(buffer, size, used, ".");
            if (fract < 10u) {
                used = append_text(buffer, size, used, "0");
            }
            used = append_u32(buffer, size, used, fract);
            used = append_text(buffer, size, used, " ");
            used = append_u32(buffer, size, used, whole);
            used = append_text(buffer, size, used, ".00\n");
            break;
        case VFS_VIRTUAL_PROC_LOADAVG:
            used = append_text(buffer, size, used, "0.00 0.00 0.00 1/1 1\n");
            break;
        case VFS_VIRTUAL_PROC_MEMINFO: {
            u32 total_kb = stats.total_bytes / 1024u;
            u32 free_kb = stats.heap_free / 1024u;
            u32 avail_kb = free_kb;

            used = append_text(buffer, size, used, "MemTotal:       ");
            used = append_u32(buffer, size, used, total_kb);
            used = append_text(buffer, size, used, " kB\nMemFree:        ");
            used = append_u32(buffer, size, used, free_kb);
            used = append_text(buffer, size, used, " kB\nMemAvailable:   ");
            used = append_u32(buffer, size, used, avail_kb);
            used = append_text(buffer, size, used, " kB\nBuffers:        0 kB\nCached:         0 kB\nShmem:          0 kB\nSReclaimable:   0 kB\nSwapTotal:      0 kB\nSwapFree:       0 kB\n");
            break;
        }
        case VFS_VIRTUAL_PROC_CPUINFO:
            used = append_text(buffer, size, used,
                               "processor\t: 0\n"
                               "vendor_id\t: OpenHobbyOS\n"
                               "model name\t: OpenHobbyOS i386 shell target\n"
                               "cpu family\t: 6\n"
                               "model\t\t: 0\n"
                               "stepping\t: 0\n"
                               "cpu MHz\t\t: 100.00\n"
                               "bogomips\t: 100.00\n"
                               "flags\t\t: fpu tsc cx8\n");
            break;
        case VFS_VIRTUAL_PROC_MOUNTS:
            used = append_text(buffer, size, used, "rootfs / initrd ro,relatime 0 0\n");
            break;
        case VFS_VIRTUAL_PROC_ROUTE:
            used = append_text(buffer,
                               size,
                               used,
                               "Iface\tDestination\tGateway\tFlags\tRefCnt\tUse\tMetric\tMask\tMTU\tWindow\tIRTT\n"
                               "lo\t00000000\t00000000\t0001\t0\t0\t0\t00000000\t0\t0\t0\n");
            break;
        case VFS_VIRTUAL_DEV_NULL:
        case VFS_VIRTUAL_DEV_TTY:
        default:
            break;
    }

    return used;
}

static u32 virtual_file_size(u32 virtual_id) {
    char buffer[1024];
    return (u32)format_virtual_file(virtual_id, buffer, sizeof(buffer));
}

static struct vfs_node *alloc_node(const char *name, struct vfs_node *parent, bool is_dir, const initrd_file_t *file) {
    struct vfs_node *node = (struct vfs_node *)kcalloc(1, sizeof(*node));
    size_t parent_len;

    if (!node) {
        panic("Out of memory while building the VFS");
    }

    node->inode = next_inode++;
    strncpy(node->name, name, sizeof(node->name) - 1);
    node->is_dir = is_dir;
    node->virtual_id = VFS_VIRTUAL_NONE;
    node->file = file;
    node->parent = parent;

    if (!parent) {
        strcpy(node->path, "/");
        return node;
    }

    parent_len = strlen(parent->path);
    if (strcmp(parent->path, "/") == 0) {
        snprintf(node->path, sizeof(node->path), "/%s", name);
    } else if (parent_len + 1 + strlen(name) + 1 <= sizeof(node->path)) {
        snprintf(node->path, sizeof(node->path), "%s/%s", parent->path, name);
    } else {
        panic("VFS path is too long");
    }

    node->sibling = parent->child;
    parent->child = node;
    return node;
}

static struct vfs_node *find_child(struct vfs_node *parent, const char *name) {
    struct vfs_node *child = parent ? parent->child : NULL;

    while (child) {
        if (strcmp(child->name, name) == 0) {
            return child;
        }
        child = child->sibling;
    }
    return NULL;
}

static struct vfs_node *ensure_child(struct vfs_node *parent, const char *name, bool is_dir, const initrd_file_t *file) {
    struct vfs_node *node = find_child(parent, name);

    if (node) {
        if (!is_dir) {
            node->file = file;
            node->is_dir = false;
        }
        return node;
    }

    return alloc_node(name, parent, is_dir, file);
}

void vfs_init(void) {
    mounted = false;
    root_node = alloc_node("/", NULL, true, NULL);

    for (u32 i = 0; i < initrd_count(); ++i) {
        const initrd_file_t *file = initrd_file_at(i);
        struct vfs_node *cursor = root_node;
        char segment[VFS_NAME_MAX];
        const char *path = file->name;
        char next[VFS_NAME_MAX];

        while (path_segment(&path, segment, sizeof(segment))) {
            const char *snapshot = path;
            bool has_more = path_segment(&snapshot, next, sizeof(next));
            cursor = ensure_child(cursor, segment, has_more, has_more ? NULL : file);
            if (!has_more) {
                break;
            }
        }
    }

    {
        struct vfs_node *proc = ensure_child(root_node, "proc", true, NULL);
        struct vfs_node *net = ensure_child(proc, "net", true, NULL);
        struct vfs_node *dev = ensure_child(root_node, "dev", true, NULL);

        ensure_child(proc, "uptime", false, NULL)->virtual_id = VFS_VIRTUAL_PROC_UPTIME;
        ensure_child(proc, "loadavg", false, NULL)->virtual_id = VFS_VIRTUAL_PROC_LOADAVG;
        ensure_child(proc, "meminfo", false, NULL)->virtual_id = VFS_VIRTUAL_PROC_MEMINFO;
        ensure_child(proc, "cpuinfo", false, NULL)->virtual_id = VFS_VIRTUAL_PROC_CPUINFO;
        ensure_child(proc, "mounts", false, NULL)->virtual_id = VFS_VIRTUAL_PROC_MOUNTS;
        ensure_child(proc, "route", false, NULL)->virtual_id = VFS_VIRTUAL_PROC_ROUTE;
        ensure_child(net, "route", false, NULL)->virtual_id = VFS_VIRTUAL_PROC_ROUTE;
        ensure_child(dev, "null", false, NULL)->virtual_id = VFS_VIRTUAL_DEV_NULL;
        ensure_child(dev, "tty", false, NULL)->virtual_id = VFS_VIRTUAL_DEV_TTY;
    }

    mounted = true;
}

bool vfs_ready(void) {
    return mounted;
}

const vfs_node_t *vfs_root(void) {
    return root_node;
}

const vfs_node_t *vfs_resolve(const vfs_node_t *cwd, const char *path) {
    const char *cursor = path;
    const struct vfs_node *node = (!path || path[0] == '/') ? root_node : (cwd ? cwd : root_node);
    char segment[VFS_NAME_MAX];

    if (!mounted || !path || !*path) {
        return node;
    }

    while (path_segment(&cursor, segment, sizeof(segment))) {
        if (strcmp(segment, ".") == 0) {
            continue;
        }
        if (strcmp(segment, "..") == 0) {
            node = node->parent ? node->parent : root_node;
            continue;
        }

        node = find_child((struct vfs_node *)node, segment);
        if (!node) {
            return NULL;
        }
    }

    return node;
}

const vfs_node_t *vfs_parent(const vfs_node_t *node) {
    if (!node || !node->parent) {
        return root_node;
    }
    return node->parent;
}

const char *vfs_name(const vfs_node_t *node) {
    return node ? node->name : "";
}

const char *vfs_path(const vfs_node_t *node) {
    return node ? node->path : "";
}

u64 vfs_inode(const vfs_node_t *node) {
    return node ? node->inode : 0;
}

bool vfs_is_dir(const vfs_node_t *node) {
    return node && node->is_dir;
}

bool vfs_is_file(const vfs_node_t *node) {
    return node && !node->is_dir && (node->file || node->virtual_id != VFS_VIRTUAL_NONE);
}

const initrd_file_t *vfs_backing_file(const vfs_node_t *node) {
    return vfs_is_file(node) ? node->file : NULL;
}

u32 vfs_file_size(const vfs_node_t *node) {
    if (!vfs_is_file(node)) {
        return 0;
    }
    if (node->virtual_id != VFS_VIRTUAL_NONE) {
        return virtual_file_size(node->virtual_id);
    }
    return node->file->size;
}

ssize_t vfs_read(const vfs_node_t *node, u32 offset, void *buffer, size_t length) {
    size_t available;
    char virtual_buffer[1024];

    if (!vfs_is_file(node)) {
        return 0;
    }

    if (node->virtual_id != VFS_VIRTUAL_NONE) {
        size_t generated = format_virtual_file(node->virtual_id, virtual_buffer, sizeof(virtual_buffer));

        if (offset >= generated) {
            return 0;
        }

        available = generated - offset;
        if (length > available) {
            length = available;
        }
        memcpy(buffer, virtual_buffer + offset, length);
        return (ssize_t)length;
    }

    if (offset >= node->file->size) {
        return 0;
    }

    available = node->file->size - offset;
    if (length > available) {
        length = available;
    }

    memcpy(buffer, node->file->data + offset, length);
    return (ssize_t)length;
}

bool vfs_stat_node(const vfs_node_t *node, vfs_stat_t *stat) {
    if (!node || !stat) {
        return false;
    }

    memset(stat, 0, sizeof(*stat));
    stat->inode = node->inode;
    stat->is_dir = node->is_dir;
    stat->block_size = 512;
    stat->size = node->is_dir ? 0 : vfs_file_size(node);
    stat->blocks = (stat->size + 511u) / 512u;
    if (node->is_dir) {
        stat->mode = LINUX_S_IFDIR | 0555u;
    } else if (node->virtual_id == VFS_VIRTUAL_DEV_NULL || node->virtual_id == VFS_VIRTUAL_DEV_TTY) {
        stat->mode = LINUX_S_IFCHR | 0666u;
        stat->block_size = 1;
    } else {
        stat->mode = LINUX_S_IFREG | (looks_executable(node->file) ? 0555u : 0444u);
    }
    return true;
}

u32 vfs_child_count(const vfs_node_t *node) {
    const struct vfs_node *child = node ? node->child : NULL;
    u32 count = 0;

    while (child) {
        count++;
        child = child->sibling;
    }

    return count;
}

const vfs_node_t *vfs_child_at(const vfs_node_t *node, u32 index) {
    const struct vfs_node *child = node ? node->child : NULL;

    while (child) {
        if (index == 0) {
            return child;
        }
        index--;
        child = child->sibling;
    }

    return NULL;
}
