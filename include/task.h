#ifndef OHOS_TASK_H
#define OHOS_TASK_H

#include "idt.h"
#include "vfs.h"
#include "types.h"

#define USER_BASE      0x01000000u
#define USER_LIMIT     0x02000000u
#define USER_STACK_TOP 0x01FF0000u
#define USER_MMAP_BASE 0x01C00000u
#define USER_MMAP_TOP  0x01F00000u
#define TASK_MAX_FDS   32
#define TASK_MAX_MMAPS 16

typedef enum {
    TASK_FD_FREE = 0,
    TASK_FD_CONSOLE,
    TASK_FD_NULL,
    TASK_FD_FILE,
    TASK_FD_DIR,
} task_fd_kind_t;

typedef struct {
    bool used;
    task_fd_kind_t kind;
    int flags;
    u32 offset;
    const vfs_node_t *node;
} task_fd_t;

typedef struct {
    bool used;
    u32 base;
    u32 length;
    int prot;
    int flags;
    const vfs_node_t *node;
    u32 file_offset;
} task_mapping_t;

typedef struct {
    char name[64];
    char path[VFS_PATH_MAX];
    char cwd_path[VFS_PATH_MAX];
    u32 entry;
    u32 brk_base;
    u32 brk;
    u32 brk_limit;
    const vfs_node_t *cwd;
    u32 pid;
    bool active;
    task_fd_t fds[TASK_MAX_FDS];
    task_mapping_t mappings[TASK_MAX_MMAPS];
} task_state_t;

void task_init(void);
bool task_can_run(void);
bool task_is_active(void);
const task_state_t *task_state(void);
int task_run_path(const char *path);
int task_run_argv(const char *path, int argc, const char *const *argv);
bool task_validate_user_range(uintptr_t base, size_t length);
bool task_copy_from_user(void *dest, const void *user_src, size_t length);
bool task_copy_to_user(void *user_dest, const void *src, size_t length);
bool task_copy_string_from_user(char *dest, size_t dest_size, const char *user_src);
u32 task_brk(u32 requested);
int task_open(const char *path, int flags, int mode);
int task_open_relative(int dirfd, const char *path, int flags, int mode);
int task_close(int fd);
ssize_t task_read_fd(int fd, void *buffer, size_t length);
ssize_t task_write_fd(int fd, const void *buffer, size_t length);
i32 task_lseek(int fd, i32 offset, int whence);
int task_dup(int oldfd, int newfd_hint);
bool task_is_console_fd(int fd);
int task_access(const char *path, int mode);
int task_stat_path(const char *path, void *user_stat, size_t stat_size);
int task_stat_fd(int fd, void *user_stat, size_t stat_size);
int task_getdents64(int fd, void *user_buffer, size_t length);
int task_getcwd(void *user_buffer, size_t length);
int task_chdir(const char *path);
void *task_mmap(void *addr, size_t length, int prot, int flags, int fd, u32 page_offset);
int task_munmap(void *addr, size_t length);
NORETURN void task_exit_current(int exit_code);
NORETURN void task_abort_from_trap(registers_t *regs, const char *reason);

extern uintptr_t task_saved_esp;
extern int task_exit_code;

int task_enter_user(u32 entry, u32 user_stack);
NORETURN void task_return_to_kernel(int exit_code);

#endif
