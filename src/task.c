#include "task.h"

#include "abi/linux.h"
#include "console.h"
#include "elf.h"
#include "format.h"
#include "gdt.h"
#include "keyboard.h"
#include "memory.h"
#include "pit.h"
#include "string.h"

#define TASK_PAGE_SIZE     4096u
#define TASK_STACK_ARG_MAX 32
#define TASK_ENV_MAX       16
#define TASK_OFFSETOF(type, member) ((u32)__builtin_offsetof(type, member))

#define LERR_PERM    (-1)
#define LERR_NOENT   (-2)
#define LERR_INTR    (-4)
#define LERR_IO      (-5)
#define LERR_BADF    (-9)
#define LERR_AGAIN   (-11)
#define LERR_NOMEM   (-12)
#define LERR_ACCES   (-13)
#define LERR_FAULT   (-14)
#define LERR_NOTDIR  (-20)
#define LERR_ISDIR   (-21)
#define LERR_INVAL   (-22)
#define LERR_MFILE   (-24)
#define LERR_NOSYS   (-38)

uintptr_t task_saved_esp;
int task_exit_code;

static task_state_t current_task;
static bool task_runtime_ready;
static u8 user_trap_stack[16384];
static u32 next_pid = 1;

static u32 align_down(u32 value, u32 alignment) {
    return value & ~(alignment - 1u);
}

static u32 align_up(u32 value, u32 alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

static bool range_overlaps(u32 base_a, u32 len_a, u32 base_b, u32 len_b) {
    u32 end_a = base_a + len_a;
    u32 end_b = base_b + len_b;
    return !(end_a <= base_b || end_b <= base_a);
}

static void task_reset_descriptors(void) {
    memset(current_task.fds, 0, sizeof(current_task.fds));
    memset(current_task.mappings, 0, sizeof(current_task.mappings));

    for (int fd = 0; fd < 3; ++fd) {
        current_task.fds[fd].used = true;
        current_task.fds[fd].kind = TASK_FD_CONSOLE;
        current_task.fds[fd].flags = (fd == 0) ? LINUX_O_RDONLY : LINUX_O_WRONLY;
        current_task.fds[fd].offset = 0;
        current_task.fds[fd].node = NULL;
    }
}

static task_fd_t *task_fd_slot(int fd) {
    if (fd < 0 || fd >= TASK_MAX_FDS) {
        return NULL;
    }
    return &current_task.fds[fd];
}

static int task_alloc_fd(void) {
    for (int fd = 3; fd < TASK_MAX_FDS; ++fd) {
        if (!current_task.fds[fd].used) {
            return fd;
        }
    }
    return LERR_MFILE;
}

static int task_install_fd(task_fd_kind_t kind, const vfs_node_t *node, int flags) {
    int fd = task_alloc_fd();

    if (fd < 0) {
        return fd;
    }

    current_task.fds[fd].used = true;
    current_task.fds[fd].kind = kind;
    current_task.fds[fd].flags = flags;
    current_task.fds[fd].offset = 0;
    current_task.fds[fd].node = node;
    return fd;
}

static const vfs_node_t *task_resolve_dirfd(int dirfd) {
    if (dirfd == LINUX_AT_FDCWD) {
        return current_task.cwd ? current_task.cwd : vfs_root();
    }

    {
        task_fd_t *fd = task_fd_slot(dirfd);
        if (!fd || !fd->used || fd->kind != TASK_FD_DIR || !fd->node) {
            return NULL;
        }
        return fd->node;
    }
}

static const vfs_node_t *task_resolve_path_internal(const vfs_node_t *base, const char *path) {
    const vfs_node_t *cwd = base ? base : current_task.cwd;
    return vfs_resolve(cwd, path);
}

static bool task_is_executable_node(const vfs_node_t *node) {
    vfs_stat_t stat;

    if (!node || !vfs_stat_node(node, &stat)) {
        return false;
    }

    return !stat.is_dir && ((stat.mode & 0111u) != 0);
}

static bool task_push_bytes(u32 *stack, const void *data, size_t length, u32 *user_address) {
    if (*stack < USER_BASE + length) {
        return false;
    }
    *stack -= (u32)length;
    memcpy((void *)(uintptr_t)(*stack), data, length);
    if (user_address) {
        *user_address = *stack;
    }
    return true;
}

static bool task_push_u32(u32 *stack, u32 value) {
    return task_push_bytes(stack, &value, sizeof(value), NULL);
}

static bool task_prepare_user_stack(int argc, const char *const *argv, u32 *user_stack) {
    static const char *const fixed_env[] = {
        "HOME=/root",
        "USER=root",
        "LOGNAME=root",
        "SHELL=/bin/sh",
        "PATH=/bin:/usr/bin",
        "TERM=openhobby",
        "LANG=C",
        "XDG_CONFIG_HOME=/root/.config",
        "XDG_CONFIG_DIRS=/etc/xdg:/etc",
        "XDG_DATA_DIRS=/usr/share:/usr/local/share",
    };
    char pwd_env[VFS_PATH_MAX + 5];
    const char *env_list[TASK_ENV_MAX];
    u32 env_ptrs[TASK_ENV_MAX];
    u32 arg_ptrs[TASK_STACK_ARG_MAX];
    const char *exec_path = current_task.path[0] ? current_task.path : "/bin/unknown";
    u32 execfn_ptr = 0;
    u32 stack = USER_STACK_TOP;
    int envc = 0;

    if (argc < 0 || argc > TASK_STACK_ARG_MAX) {
        return false;
    }

    snprintf(pwd_env, sizeof(pwd_env), "PWD=%s", current_task.cwd_path[0] ? current_task.cwd_path : "/");

    for (size_t i = 0; i < sizeof(fixed_env) / sizeof(fixed_env[0]); ++i) {
        env_list[envc++] = fixed_env[i];
    }
    env_list[envc++] = pwd_env;

    for (int i = envc - 1; i >= 0; --i) {
        size_t len = strlen(env_list[i]) + 1;
        if (!task_push_bytes(&stack, env_list[i], len, &env_ptrs[i])) {
            return false;
        }
    }

    for (int i = argc - 1; i >= 0; --i) {
        size_t len = strlen(argv[i]) + 1;
        if (!task_push_bytes(&stack, argv[i], len, &arg_ptrs[i])) {
            return false;
        }
    }

    if (!task_push_bytes(&stack, exec_path, strlen(exec_path) + 1, &execfn_ptr)) {
        return false;
    }

    stack = align_down(stack, 16);

    if (!task_push_u32(&stack, 0) ||
        !task_push_u32(&stack, LINUX_AT_NULL) ||
        !task_push_u32(&stack, execfn_ptr) ||
        !task_push_u32(&stack, LINUX_AT_EXECFN) ||
        !task_push_u32(&stack, current_task.entry) ||
        !task_push_u32(&stack, LINUX_AT_ENTRY) ||
        !task_push_u32(&stack, 0) ||
        !task_push_u32(&stack, LINUX_AT_EGID) ||
        !task_push_u32(&stack, 0) ||
        !task_push_u32(&stack, LINUX_AT_GID) ||
        !task_push_u32(&stack, 0) ||
        !task_push_u32(&stack, LINUX_AT_EUID) ||
        !task_push_u32(&stack, 0) ||
        !task_push_u32(&stack, LINUX_AT_UID) ||
        !task_push_u32(&stack, TASK_PAGE_SIZE) ||
        !task_push_u32(&stack, LINUX_AT_PAGESZ)) {
        return false;
    }

    if (!task_push_u32(&stack, 0)) {
        return false;
    }
    for (int i = envc - 1; i >= 0; --i) {
        if (!task_push_u32(&stack, env_ptrs[i])) {
            return false;
        }
    }

    if (!task_push_u32(&stack, 0)) {
        return false;
    }
    for (int i = argc - 1; i >= 0; --i) {
        if (!task_push_u32(&stack, arg_ptrs[i])) {
            return false;
        }
    }

    if (!task_push_u32(&stack, (u32)argc)) {
        return false;
    }

    *user_stack = align_down(stack, 16);
    return true;
}

static void task_fill_stat64(const vfs_node_t *node, struct linux_stat64 *stat) {
    vfs_stat_t info;

    memset(stat, 0, sizeof(*stat));
    vfs_stat_node(node, &info);

    stat->st_dev = 1;
    stat->__st_ino = (u32)info.inode;
    stat->st_mode = info.mode;
    stat->st_nlink = info.is_dir ? 2 : 1;
    stat->st_uid = 0;
    stat->st_gid = 0;
    stat->st_rdev = 0;
    stat->st_size = info.size;
    stat->st_blksize = info.block_size;
    stat->st_blocks = info.blocks;
    stat->st_atime = pit_ticks() / pit_frequency();
    stat->st_mtime = stat->st_atime;
    stat->st_ctime = stat->st_atime;
    stat->st_ino = info.inode;
}

static ssize_t task_console_read(void *buffer, size_t length) {
    char *out = (char *)buffer;
    size_t used = 0;

    if (length == 0) {
        return 0;
    }

    for (;;) {
        char ch = keyboard_getchar();

        if (ch == '\r') {
            ch = '\n';
        }

        if (ch == '\b') {
            if (used) {
                used--;
                console_putc('\b');
            }
            continue;
        }

        out[used++] = ch;
        console_putc(ch);
        if (ch == '\n' || used >= length) {
            return (ssize_t)used;
        }
    }
}

static void task_clear_user_space(void) {
    memset((void *)(uintptr_t)USER_BASE, 0, USER_LIMIT - USER_BASE);
}

static task_mapping_t *task_mapping_find(void *addr, size_t length) {
    u32 base = (u32)(uintptr_t)addr;
    u32 size = align_up((u32)length, TASK_PAGE_SIZE);

    for (int i = 0; i < TASK_MAX_MMAPS; ++i) {
        if (current_task.mappings[i].used &&
            current_task.mappings[i].base == base &&
            current_task.mappings[i].length == size) {
            return &current_task.mappings[i];
        }
    }
    return NULL;
}

static bool task_mapping_overlaps(u32 base, u32 length) {
    for (int i = 0; i < TASK_MAX_MMAPS; ++i) {
        if (current_task.mappings[i].used &&
            range_overlaps(base, length, current_task.mappings[i].base, current_task.mappings[i].length)) {
            return true;
        }
    }

    if (range_overlaps(base, length, USER_BASE, current_task.brk)) {
        return true;
    }

    return false;
}

void task_init(void) {
    memset(&current_task, 0, sizeof(current_task));
    task_saved_esp = 0;
    task_exit_code = 0;
    task_runtime_ready = memory_total_bytes() > USER_LIMIT && vfs_ready();
    current_task.cwd = vfs_root();
    strcpy(current_task.cwd_path, "/");
    current_task.pid = next_pid++;
    task_reset_descriptors();
    gdt_set_kernel_stack((uintptr_t)(user_trap_stack + sizeof(user_trap_stack)));
}

bool task_can_run(void) {
    return task_runtime_ready && vfs_ready();
}

bool task_is_active(void) {
    return current_task.active;
}

const task_state_t *task_state(void) {
    return &current_task;
}

bool task_validate_user_range(uintptr_t base, size_t length) {
    uintptr_t end = base + length;
    if (base < USER_BASE || end > USER_LIMIT || end < base) {
        return false;
    }
    return true;
}

bool task_copy_from_user(void *dest, const void *user_src, size_t length) {
    if (!task_validate_user_range((uintptr_t)user_src, length)) {
        return false;
    }
    memcpy(dest, user_src, length);
    return true;
}

bool task_copy_to_user(void *user_dest, const void *src, size_t length) {
    if (!task_validate_user_range((uintptr_t)user_dest, length)) {
        return false;
    }
    memcpy(user_dest, src, length);
    return true;
}

bool task_copy_string_from_user(char *dest, size_t dest_size, const char *user_src) {
    uintptr_t base = (uintptr_t)user_src;
    size_t used = 0;

    if (dest_size == 0 || base < USER_BASE || base >= USER_LIMIT) {
        return false;
    }

    while (base + used < USER_LIMIT && used + 1 < dest_size) {
        char ch = *(const char *)(uintptr_t)(base + used);
        dest[used++] = ch;
        if (ch == '\0') {
            return true;
        }
    }

    dest[dest_size - 1] = '\0';
    return false;
}

u32 task_brk(u32 requested) {
    if (!current_task.active && !current_task.brk) {
        return 0;
    }

    if (requested == 0) {
        return current_task.brk;
    }

    if (requested < current_task.brk_base || requested >= current_task.brk_limit) {
        return current_task.brk;
    }

    if (task_mapping_overlaps(current_task.brk, requested - current_task.brk)) {
        return current_task.brk;
    }

    if (requested > current_task.brk) {
        memset((void *)(uintptr_t)current_task.brk, 0, requested - current_task.brk);
    }

    current_task.brk = requested;
    return current_task.brk;
}

int task_run_path(const char *path) {
    const char *argv[2];
    argv[0] = path;
    argv[1] = NULL;
    return task_run_argv(path, 1, argv);
}

int task_run_argv(const char *path, int argc, const char *const *argv) {
    const vfs_node_t *node;
    elf_image_t image;
    u32 user_stack;

    if (!task_can_run() || !path || argc <= 0 || !argv) {
        return -1;
    }

    node = task_resolve_path_internal(vfs_root(), path);
    if (!node || !vfs_is_file(node) || !task_is_executable_node(node)) {
        return LERR_NOENT;
    }

    task_clear_user_space();
    if (!elf_load_image(vfs_backing_file(node), USER_BASE, USER_LIMIT, &image)) {
        return -1;
    }

    memset(&current_task, 0, sizeof(current_task));
    strncpy(current_task.name, vfs_name(node), sizeof(current_task.name) - 1);
    strncpy(current_task.path, vfs_path(node), sizeof(current_task.path) - 1);
    current_task.cwd = vfs_root();
    strcpy(current_task.cwd_path, "/");
    current_task.entry = image.entry;
    current_task.brk_base = image.image_end;
    current_task.brk = image.image_end;
    current_task.brk_limit = USER_MMAP_BASE;
    current_task.pid = next_pid++;
    current_task.active = true;
    task_exit_code = 0;
    task_reset_descriptors();

    if (!task_prepare_user_stack(argc, argv, &user_stack)) {
        current_task.active = false;
        return LERR_NOMEM;
    }

    task_exit_code = task_enter_user(current_task.entry, user_stack);
    current_task.active = false;
    current_task.cwd = vfs_root();
    strcpy(current_task.cwd_path, "/");
    return task_exit_code;
}

int task_open_relative(int dirfd, const char *path, int flags, int mode) {
    const vfs_node_t *base;
    const vfs_node_t *node;
    const char *node_path;
    bool want_write = (flags & LINUX_O_WRONLY) || (flags & LINUX_O_RDWR);

    (void)mode;

    if (!path || !*path) {
        return LERR_INVAL;
    }

    base = task_resolve_dirfd(dirfd);
    if (!base) {
        return LERR_BADF;
    }

    node = task_resolve_path_internal(path[0] == '/' ? vfs_root() : base, path);
    if (!node) {
        return LERR_NOENT;
    }

    node_path = vfs_path(node);
    if (strcmp(node_path, "/dev/tty") == 0) {
        return task_install_fd(TASK_FD_CONSOLE, node, flags);
    }
    if (strcmp(node_path, "/dev/null") == 0) {
        return task_install_fd(TASK_FD_NULL, node, flags);
    }

    if (want_write) {
        return LERR_ACCES;
    }

    if ((flags & LINUX_O_DIRECTORY) && !vfs_is_dir(node)) {
        return LERR_NOTDIR;
    }

    if (vfs_is_dir(node)) {
        return task_install_fd(TASK_FD_DIR, node, flags);
    }

    return task_install_fd(TASK_FD_FILE, node, flags);
}

int task_open(const char *path, int flags, int mode) {
    return task_open_relative(LINUX_AT_FDCWD, path, flags, mode);
}

int task_close(int fd) {
    task_fd_t *slot = task_fd_slot(fd);

    if (!slot || !slot->used) {
        return LERR_BADF;
    }
    if (fd < 3 && slot->kind == TASK_FD_CONSOLE) {
        return 0;
    }

    memset(slot, 0, sizeof(*slot));
    return 0;
}

ssize_t task_read_fd(int fd, void *buffer, size_t length) {
    task_fd_t *slot = task_fd_slot(fd);

    if (!slot || !slot->used) {
        return LERR_BADF;
    }

    if (slot->kind == TASK_FD_CONSOLE) {
        if ((slot->flags & LINUX_O_WRONLY) != 0) {
            return LERR_BADF;
        }
        return task_console_read(buffer, length);
    }

    if (slot->kind == TASK_FD_NULL) {
        return 0;
    }

    if (slot->kind == TASK_FD_DIR) {
        return LERR_ISDIR;
    }

    if (slot->kind == TASK_FD_FILE && slot->node) {
        ssize_t read_count = vfs_read(slot->node, slot->offset, buffer, length);
        if (read_count > 0) {
            slot->offset += (u32)read_count;
        }
        return read_count;
    }

    return LERR_BADF;
}

ssize_t task_write_fd(int fd, const void *buffer, size_t length) {
    task_fd_t *slot = task_fd_slot(fd);

    if (!slot || !slot->used) {
        return LERR_BADF;
    }

    if (slot->kind == TASK_FD_CONSOLE) {
        if ((slot->flags & LINUX_O_RDONLY) != 0) {
            return LERR_BADF;
        }
        console_write_buffer((const char *)buffer, length);
        return (ssize_t)length;
    }

    if (slot->kind == TASK_FD_NULL) {
        return (ssize_t)length;
    }

    return LERR_ACCES;
}

i32 task_lseek(int fd, i32 offset, int whence) {
    task_fd_t *slot = task_fd_slot(fd);
    u32 size = 0;
    i32 next = 0;

    if (!slot || !slot->used) {
        return LERR_BADF;
    }
    if (slot->kind == TASK_FD_CONSOLE) {
        return LERR_INVAL;
    }

    size = (slot->kind == TASK_FD_DIR) ? (vfs_child_count(slot->node) + 2u) : vfs_file_size(slot->node);

    if (whence == LINUX_SEEK_SET) {
        next = offset;
    } else if (whence == LINUX_SEEK_CUR) {
        next = (i32)slot->offset + offset;
    } else if (whence == LINUX_SEEK_END) {
        next = (i32)size + offset;
    } else {
        return LERR_INVAL;
    }

    if (next < 0) {
        return LERR_INVAL;
    }

    slot->offset = (u32)next;
    return next;
}

int task_dup(int oldfd, int newfd_hint) {
    task_fd_t *old_slot = task_fd_slot(oldfd);
    int newfd = newfd_hint;

    if (!old_slot || !old_slot->used) {
        return LERR_BADF;
    }

    if (newfd < 0) {
        newfd = task_alloc_fd();
    }
    if (newfd < 0) {
        return newfd;
    }

    if (newfd >= TASK_MAX_FDS) {
        return LERR_BADF;
    }

    current_task.fds[newfd] = *old_slot;
    return newfd;
}

bool task_is_console_fd(int fd) {
    task_fd_t *slot = task_fd_slot(fd);
    return slot && slot->used && slot->kind == TASK_FD_CONSOLE;
}

int task_access(const char *path, int mode) {
    const vfs_node_t *node = task_resolve_path_internal(current_task.cwd, path);

    if (!node) {
        return LERR_NOENT;
    }
    if ((mode & LINUX_W_OK) != 0) {
        return LERR_ACCES;
    }
    if ((mode & LINUX_X_OK) != 0 && !vfs_is_dir(node) && !task_is_executable_node(node)) {
        return LERR_ACCES;
    }
    return 0;
}

int task_stat_path(const char *path, void *user_stat, size_t stat_size) {
    const vfs_node_t *node = task_resolve_path_internal(current_task.cwd, path);
    struct linux_stat64 stat;

    if (!node) {
        return LERR_NOENT;
    }
    if (stat_size < sizeof(stat)) {
        return LERR_INVAL;
    }

    task_fill_stat64(node, &stat);
    if (!task_copy_to_user(user_stat, &stat, sizeof(stat))) {
        return LERR_FAULT;
    }
    return 0;
}

int task_stat_fd(int fd, void *user_stat, size_t stat_size) {
    task_fd_t *slot = task_fd_slot(fd);
    struct linux_stat64 stat;

    if (!slot || !slot->used) {
        return LERR_BADF;
    }
    if (slot->kind == TASK_FD_CONSOLE || slot->kind == TASK_FD_NULL) {
        memset(&stat, 0, sizeof(stat));
        stat.st_mode = LINUX_S_IFCHR | 0666u;
        stat.st_nlink = 1;
        stat.st_blksize = 1;
    } else {
        task_fill_stat64(slot->node, &stat);
    }

    if (stat_size < sizeof(stat) || !task_copy_to_user(user_stat, &stat, sizeof(stat))) {
        return LERR_FAULT;
    }
    return 0;
}

int task_getdents64(int fd, void *user_buffer, size_t length) {
    task_fd_t *slot = task_fd_slot(fd);
    u8 *out = (u8 *)user_buffer;
    size_t used = 0;

    if (!slot || !slot->used || slot->kind != TASK_FD_DIR || !slot->node) {
        return LERR_NOTDIR;
    }

    while (used + 24 < length) {
        const vfs_node_t *entry = NULL;
        const char *name = NULL;
        u8 dtype = LINUX_DT_UNKNOWN;
        u8 record[sizeof(struct linux_dirent64)];
        size_t name_len;
        u16 reclen;
        u64 ino;
        i64 next_off;

        if (slot->offset == 0) {
            entry = slot->node;
            name = ".";
        } else if (slot->offset == 1) {
            entry = vfs_parent(slot->node);
            name = "..";
        } else {
            entry = vfs_child_at(slot->node, slot->offset - 2u);
            if (!entry) {
                break;
            }
            name = vfs_name(entry);
        }

        name_len = strlen(name) + 1;
        reclen = (u16)align_up((u32)(TASK_OFFSETOF(struct linux_dirent64, d_name) + name_len), 8);
        if (used + reclen > length) {
            break;
        }

        memset(record, 0, sizeof(record));
        ino = vfs_inode(entry);
        next_off = (i64)(slot->offset + 1u);
        dtype = vfs_is_dir(entry) ? LINUX_DT_DIR : LINUX_DT_REG;

        memcpy(record + TASK_OFFSETOF(struct linux_dirent64, d_ino), &ino, sizeof(ino));
        memcpy(record + TASK_OFFSETOF(struct linux_dirent64, d_off), &next_off, sizeof(next_off));
        memcpy(record + TASK_OFFSETOF(struct linux_dirent64, d_reclen), &reclen, sizeof(reclen));
        record[TASK_OFFSETOF(struct linux_dirent64, d_type)] = dtype;
        memcpy(record + TASK_OFFSETOF(struct linux_dirent64, d_name), name, name_len);

        if (!task_copy_to_user(out + used, record, reclen)) {
            return LERR_FAULT;
        }

        used += reclen;
        slot->offset++;
    }

    return (int)used;
}

int task_getcwd(void *user_buffer, size_t length) {
    const char *cwd = current_task.cwd_path[0] ? current_task.cwd_path : "/";
    size_t needed = strlen(cwd) + 1;

    if (length < needed) {
        return LERR_INVAL;
    }
    if (!task_copy_to_user(user_buffer, cwd, needed)) {
        return LERR_FAULT;
    }
    return (int)needed;
}

int task_chdir(const char *path) {
    const vfs_node_t *node = task_resolve_path_internal(current_task.cwd, path);

    if (!node) {
        return LERR_NOENT;
    }
    if (!vfs_is_dir(node)) {
        return LERR_NOTDIR;
    }

    current_task.cwd = node;
    strncpy(current_task.cwd_path, vfs_path(node), sizeof(current_task.cwd_path) - 1);
    return 0;
}

void *task_mmap(void *addr, size_t length, int prot, int flags, int fd, u32 page_offset) {
    u32 size = align_up((u32)length, TASK_PAGE_SIZE);
    u32 base = 0;
    task_fd_t *slot = NULL;
    const vfs_node_t *node = NULL;
    task_mapping_t *mapping = NULL;

    if (size == 0 || ((flags & LINUX_MAP_SHARED) == 0 && (flags & LINUX_MAP_PRIVATE) == 0)) {
        return (void *)(uintptr_t)LERR_INVAL;
    }

    if ((flags & LINUX_MAP_ANONYMOUS) == 0) {
        slot = task_fd_slot(fd);
        if (!slot || !slot->used || slot->kind != TASK_FD_FILE) {
            return (void *)(uintptr_t)LERR_BADF;
        }
        node = slot->node;
    }

    for (int i = 0; i < TASK_MAX_MMAPS; ++i) {
        if (!current_task.mappings[i].used) {
            mapping = &current_task.mappings[i];
            break;
        }
    }
    if (!mapping) {
        return (void *)(uintptr_t)LERR_NOMEM;
    }

    if ((flags & LINUX_MAP_FIXED) != 0) {
        base = align_down((u32)(uintptr_t)addr, TASK_PAGE_SIZE);
        if (base < USER_MMAP_BASE || base + size > USER_MMAP_TOP || task_mapping_overlaps(base, size)) {
            return (void *)(uintptr_t)LERR_INVAL;
        }
    } else {
        for (base = USER_MMAP_TOP - size; base >= USER_MMAP_BASE; base -= TASK_PAGE_SIZE) {
            if (!task_mapping_overlaps(base, size)) {
                break;
            }
            if (base == USER_MMAP_BASE) {
                break;
            }
        }
        if (base < USER_MMAP_BASE || task_mapping_overlaps(base, size)) {
            return (void *)(uintptr_t)LERR_NOMEM;
        }
    }

    memset((void *)(uintptr_t)base, 0, size);
    if (node) {
        u32 file_offset = page_offset * TASK_PAGE_SIZE;
        if (file_offset < vfs_file_size(node)) {
            ssize_t copied = vfs_read(node, file_offset, (void *)(uintptr_t)base, size);
            if (copied < 0) {
                return (void *)(uintptr_t)LERR_IO;
            }
        }
    }

    memset(mapping, 0, sizeof(*mapping));
    mapping->used = true;
    mapping->base = base;
    mapping->length = size;
    mapping->prot = prot;
    mapping->flags = flags;
    mapping->node = node;
    mapping->file_offset = page_offset * TASK_PAGE_SIZE;
    return (void *)(uintptr_t)base;
}

int task_munmap(void *addr, size_t length) {
    task_mapping_t *mapping = task_mapping_find(addr, length);

    if (!mapping) {
        return LERR_INVAL;
    }

    memset((void *)(uintptr_t)mapping->base, 0, mapping->length);
    memset(mapping, 0, sizeof(*mapping));
    return 0;
}

NORETURN void task_exit_current(int exit_code) {
    current_task.active = false;
    task_return_to_kernel(exit_code);
}

NORETURN void task_abort_from_trap(registers_t *regs, const char *reason) {
    console_printf("\n[task] %s hit %s at eip=%x err=%x\n",
                   current_task.path[0] ? current_task.path : "(unnamed)",
                   reason,
                   regs->eip,
                   regs->err_code);
    current_task.active = false;
    task_return_to_kernel(128 + (int)regs->int_no);
}
