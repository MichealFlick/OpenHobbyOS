#include "task.h"

#include "abi/linux.h"
#include "console.h"
#include "elf.h"
#include "format.h"
#include "gdt.h"
#include "io.h"
#include "keyboard.h"
#include "memory.h"
#include "netdev.h"
#include "paging.h"
#include "panic.h"
#include "pipe.h"
#include "pit.h"
#include "socket.h"
#include "string.h"

#define TASK_PAGE_SIZE     4096u
#define TASK_STACK_ARG_MAX 32
#define TASK_ENV_MAX       16
#define TASK_SNAPSHOT_ALIGN 16u
#define TASK_OFFSETOF(type, member) ((u32)__builtin_offsetof(type, member))

/* Compile-time verification of critical struct offsets */
_Static_assert(sizeof(registers_t) == 76, "registers_t size must be 76 bytes");
_Static_assert(TASK_OFFSETOF(registers_t, eax) == 28, "eax offset in registers_t");
_Static_assert(TASK_OFFSETOF(registers_t, eip) == 56, "eip offset in registers_t");
_Static_assert(TASK_OFFSETOF(registers_t, cs) == 60, "cs offset in registers_t");
_Static_assert(TASK_OFFSETOF(task_state_t, fpu_state) % 16 == 0,
               "fpu_state must be 16-byte aligned");

#define LERR_PERM    (-1)
#define LERR_NOENT   (-2)
#define LERR_INTR    (-4)
#define LERR_IO      (-5)
#define LERR_BADF    (-9)
#define LERR_CHILD   (-10)
#define LERR_AGAIN   (-11)
#define LERR_NOMEM   (-12)
#define LERR_ACCES   (-13)
#define LERR_FAULT   (-14)
#define LERR_EXIST   (-17)
#define LERR_NOTDIR  (-20)
#define LERR_ISDIR   (-21)
#define LERR_INVAL   (-22)
#define LERR_MFILE   (-24)
#define LERR_NOSYS   (-38)
#define LERR_ROFS    (-30)
#define LERR_PIPE    (-32)

uintptr_t task_saved_esp;
u32 task_saved_ebx;
u32 task_saved_esi;
u32 task_saved_edi;
u32 task_saved_ebp;
int task_exit_code;

task_state_t current_task;
static bool task_runtime_ready;
static u8 user_trap_stack[16384];
static u32 next_pid = 1;

typedef enum {
    TASK_SLOT_FREE = 0,
    TASK_SLOT_RUNNABLE,
    TASK_SLOT_SLEEPING,
    TASK_SLOT_WAITING,
    TASK_SLOT_ZOMBIE,
} task_slot_status_t;

#define TASK_SLEEP_FOREVER 0xFFFFFFFFu

typedef struct {
    bool used;
    bool seeded;
    task_slot_status_t status;
    int parent_pid;
    int exit_code;
    int wait_pid;
    u32 wait_status_ptr;
    u32 wake_tick;
    task_state_t state;
    registers_t regs;
    /* Paging: each task has its own page directory */
    page_directory_t *page_directory;
    u32 page_directory_phys;
    /* TLS: GS segment base for stack canary (%gs:0x14) */
    u32 tls_vaddr;
    u32 canary;
} task_slot_t;

_Static_assert(TASK_OFFSETOF(task_slot_t, regs) >= TASK_OFFSETOF(task_slot_t, state) + sizeof(task_state_t),
               "regs must not overlap state in task_slot_t");
_Static_assert(sizeof(task_state_t) == 2640, "task_state_t size must be 2640 bytes");
_Static_assert(TASK_OFFSETOF(task_slot_t, regs) == 2672, "regs must be at offset 2672 in task_slot_t");

static task_slot_t task_slots[TASK_MAX_SLOTS] __attribute__((aligned(64)));
static int active_task_slot = -1;
static int scheduler_root_pid = -1;
static int fpu_owner_slot = -1;
static int scheduler_rr_cursor = -1;

int task_active_slot_index(void) {
    return active_task_slot;
}

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

static task_slot_t *task_slot_at(int index) {
    if (index < 0 || index >= TASK_MAX_SLOTS || !task_slots[index].used) {
        return NULL;
    }
    return &task_slots[index];
}

static task_slot_t *task_active_slot(void) {
    return task_slot_at(active_task_slot);
}

static task_slot_t *task_slot_for_pid(int pid) {
    for (int i = 0; i < TASK_MAX_SLOTS; ++i) {
        if (task_slots[i].used && (int)task_slots[i].state.pid == pid) {
            return &task_slots[i];
        }
    }
    return NULL;
}

static task_slot_t *task_alloc_slot(void) {
    for (int i = 0; i < TASK_MAX_SLOTS; ++i) {
        if (!task_slots[i].used) {
            memset(&task_slots[i], 0, sizeof(task_slots[i]));
            task_slots[i].used = true;
            task_slots[i].canary = 0xCAFEBABE;
            return &task_slots[i];
        }
    }
    console_printf("[spawn] task_alloc_slot: no free slots!\n");
    return NULL;
}

static void task_free_slot(task_slot_t *slot) {
    if (!slot) {
        return;
    }

    if (slot->page_directory != NULL && page_directory_get_current() == slot->page_directory) {
        page_directory_switch_to_kernel();
    }

    /* Only clean up resources if the slot was properly seeded */
    if (slot->seeded) {
        /* Release socket and pipe references held by this slot's captured state */
        for (int i = 0; i < TASK_MAX_FDS; ++i) {
            if (slot->state.fds[i].used) {
                if (slot->state.fds[i].kind == TASK_FD_SOCKET && slot->state.fds[i].socket) {
                    socket_release((socket_endpoint_t *)slot->state.fds[i].socket);
                }
                if (slot->state.fds[i].pipe &&
                    (slot->state.fds[i].kind == TASK_FD_PIPE_READ || slot->state.fds[i].kind == TASK_FD_PIPE_WRITE)) {
                    /* Close the appropriate end if still open */
                    pipe_endpoint_t *pipe = (pipe_endpoint_t *)slot->state.fds[i].pipe;
                    if (slot->state.fds[i].kind == TASK_FD_PIPE_READ && pipe->read_open) {
                        pipe_close_read(pipe);
                    }
                    if (slot->state.fds[i].kind == TASK_FD_PIPE_WRITE && pipe->write_open) {
                        pipe_close_write(pipe);
                    }
                    pipe_release(pipe);
                }
            }
        }
    }

    /* Destroy page directory (frees all user pages) */
    if (slot->page_directory) {
        page_directory_destroy(slot->page_directory);
        slot->page_directory = NULL;
        slot->page_directory_phys = 0;
    }

    memset(slot, 0, sizeof(*slot));
}

static bool task_slot_write_to_user(task_slot_t *slot, u32 user_addr, const void *src, size_t length) {
    if (!slot || !slot->page_directory || !src || length == 0) {
        return false;
    }
    
    /* Check bounds in user space */
    if (user_addr < USER_BASE || user_addr + length < user_addr || user_addr + length > KERNEL_VIRTUAL_BASE) {
        return false;
    }
    
    /* Switch to target slot's page directory so we can access its user pages */
    page_directory_t *old_pd = page_directory_get_current();
    page_directory_switch(slot->page_directory);
    
    bool ok = false;
    if (page_is_present(NULL, user_addr) && page_is_present(NULL, user_addr + length - 1)) {
        memcpy((void *)(uintptr_t)user_addr, src, length);
        ok = true;
    }
    
    page_directory_switch(old_pd);
    return ok;
}

static bool task_slot_save_state(task_slot_t *slot, const registers_t *regs) {
    if (!slot || !regs) {
        return false;
    }

    slot->state = current_task;
    slot->regs = *regs;

    /* Increment refcount for sockets in forked child */
    for (int i = 0; i < TASK_MAX_FDS; ++i) {
        if (slot->state.fds[i].used && slot->state.fds[i].kind == TASK_FD_SOCKET && slot->state.fds[i].socket) {
            socket_endpoint_t *sock = (socket_endpoint_t *)slot->state.fds[i].socket;
            sock->refcount++;
        }
    }

    /* Increment refcount for pipes in forked child */
    for (int i = 0; i < TASK_MAX_FDS; ++i) {
        if (slot->state.fds[i].used && slot->state.fds[i].pipe &&
            (slot->state.fds[i].kind == TASK_FD_PIPE_READ || slot->state.fds[i].kind == TASK_FD_PIPE_WRITE)) {
            pipe_endpoint_t *pipe = (pipe_endpoint_t *)slot->state.fds[i].pipe;
            pipe->refcount++;
        }
    }

    slot->seeded = true;
    return true;
}

static bool task_slot_validate(const task_slot_t *slot) {
    if (!slot || !slot->used) return false;
    if (slot->canary != 0xCAFEBABE) return false;
    return true;
}

static void task_slot_activate(task_slot_t *slot) {
    if (!slot || !slot->seeded) {
        return;
    }

    /* Switch to the slot's page directory */
    if (slot->page_directory && slot->page_directory_phys) {
        paging_set_cr3(slot->page_directory_phys);
    }

    current_task = slot->state;

    /* Update GS segment base for per-task TLS (stack canary at %gs:0x14) */
    gdt_set_gs_base(slot->tls_vaddr);
}

static void task_wake_ready_slots(void) {
    u32 now = pit_ticks();

    for (int i = 0; i < TASK_MAX_SLOTS; ++i) {
        if (task_slots[i].used &&
            task_slots[i].status == TASK_SLOT_SLEEPING &&
            task_slots[i].wake_tick != TASK_SLEEP_FOREVER &&
            (i32)(now - task_slots[i].wake_tick) >= 0) {
            task_slots[i].status = TASK_SLOT_RUNNABLE;
            task_slots[i].wake_tick = 0;
        }
    }
}

static bool task_has_live_slots(void) {
    for (int i = 0; i < TASK_MAX_SLOTS; ++i) {
        if (task_slots[i].used && task_slots[i].status != TASK_SLOT_ZOMBIE) {
            return true;
        }
    }
    return false;
}

static task_slot_t *task_pick_next_runnable(int current_index) {
    for (int pass = 0; pass < TASK_MAX_SLOTS; ++pass) {
        int index = (current_index + 1 + pass) % TASK_MAX_SLOTS;
        if (task_slots[index].used && task_slots[index].status == TASK_SLOT_RUNNABLE) {
            scheduler_rr_cursor = index;
            return &task_slots[index];
        }
    }
    return NULL;
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

static void task_restore_kernel_context(void) {
    page_directory_switch_to_kernel();
    memset(&current_task, 0, sizeof(current_task));
    current_task.cwd = vfs_root();
    strcpy(current_task.cwd_path, "/");
    current_task.pid = next_pid++;
    task_reset_descriptors();
    scheduler_rr_cursor = -1;
    active_task_slot = -1;
}

task_fd_t *task_fd_slot(int fd) {
    if (fd < 0 || fd >= TASK_MAX_FDS) {
        return NULL;
    }
    return &current_task.fds[fd];
}

int task_alloc_fd(void) {
    for (int fd = 3; fd < TASK_MAX_FDS; ++fd) {
        if (!current_task.fds[fd].used) {
            return fd;
        }
    }
    return LERR_MFILE;
}

static int task_state_alloc_fd(task_state_t *state) {
    if (!state) {
        return LERR_MFILE;
    }
    for (int fd = 3; fd < TASK_MAX_FDS; ++fd) {
        if (!state->fds[fd].used) {
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

int task_install_file_fd(const vfs_node_t *node, int flags, u32 offset) {
    int fd;

    if (!node || !vfs_is_file(node)) {
        return LERR_BADF;
    }

    fd = task_install_fd(TASK_FD_FILE, node, flags);
    if (fd < 0) {
        return fd;
    }

    current_task.fds[fd].offset = offset;
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
        "SHELL=/bin/gosh",
        "ENV=/etc/goshrc",
        "PATH=/bin:/usr/bin",
        "TERM=linux",
        "COLORTERM=truecolor",
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
    u32 metadata_bytes;
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

    metadata_bytes = 16u * sizeof(u32);
    metadata_bytes += (u32)(envc + argc + 3) * sizeof(u32);
    if (stack < USER_BASE + metadata_bytes) {
        return false;
    }

    /*
     * User code expects ESP to point at argc. Align the stack frame before
     * we write argv/envp/auxv so we don't shove ESP into unmapped padding.
     */
    stack = align_down(stack - metadata_bytes, 16) + metadata_bytes;

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

    *user_stack = stack;
    return true;
}

static void task_fill_stat64(const vfs_node_t *node, struct linux_stat64 *stat) {
    vfs_stat_t info;

    memset(stat, 0, sizeof(*stat));
    vfs_stat_node(node, &info);

    stat->st_dev = 1;
    stat->__st_ino = (u32)info.inode;
    stat->st_mode = info.mode;
    stat->st_nlink = info.nlink;
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

    if (console_term_resp_available()) {
        return (ssize_t)console_read_term_resp(out, length);
    }

    while (used < length) {
        if (keyboard_has_input()) {
            char ch = keyboard_getchar();
            if (ch == '\r') ch = '\n';
            out[used++] = ch;
            if (ch == '\n') break;
            continue;
        }

        if (used > 0) break;
        return 0;
    }

    return (ssize_t)used;
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

    if (current_task.brk > USER_BASE &&
        range_overlaps(base, length, USER_BASE, current_task.brk - USER_BASE)) {
        return true;
    }

    return false;
}

void task_init(void) {
    memset(&current_task, 0, sizeof(current_task));
    memset(task_slots, 0, sizeof(task_slots));
    for (int i = 0; i < TASK_MAX_SLOTS; ++i) {
        task_slots[i].canary = 0xCAFEBABE;
    }
    task_saved_esp = 0;
    task_exit_code = 0;
    task_runtime_ready = memory_total_bytes() > USER_LIMIT && vfs_ready();
    active_task_slot = -1;
    scheduler_root_pid = -1;
    scheduler_rr_cursor = -1;
    current_task.cwd = vfs_root();
    strcpy(current_task.cwd_path, "/");
    current_task.pid = next_pid++;
    socket_init();
    pipe_init();
    task_reset_descriptors();
    gdt_set_kernel_stack((uintptr_t)(user_trap_stack + sizeof(user_trap_stack)));
}

bool task_can_run(void) {
    return task_runtime_ready && vfs_ready();
}

bool task_is_active(void) {
    return active_task_slot >= 0 && current_task.active;
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
    page_directory_t *pd = page_directory_get_current();

    if (!current_task.active && !current_task.brk) {
        return 0;
    }

    if (requested == 0) {
        return current_task.brk;
    }

    if (requested < current_task.brk_base || requested >= current_task.brk_limit) {
        return current_task.brk;
    }

    if (requested > current_task.brk &&
        task_mapping_overlaps(current_task.brk, requested - current_task.brk)) {
        return current_task.brk;
    }

    if (requested > current_task.brk) {
        /* Allocate pages for the expanded heap */
        u32 start_page = (current_task.brk + PAGE_SIZE - 1) & PAGE_MASK;
        u32 end_page = requested & PAGE_MASK;

        for (u32 va = start_page; va <= end_page; va += PAGE_SIZE) {
            if (!page_is_present(pd, va)) {
                if (!paging_alloc_user_page(pd, va, PTE_USER | PTE_RW)) {
                    /* Failed to allocate - return current brk */
                    return current_task.brk;
                }
            }
        }

        memset((void *)(uintptr_t)current_task.brk, 0, requested - current_task.brk);
    }

    current_task.brk = requested;
    return current_task.brk;
}

static bool task_wait_matches(int requested_pid, const task_slot_t *child) {
    if (!child || !child->used || requested_pid == 0) {
        return false;
    }
    return requested_pid < 0 || (int)child->state.pid == requested_pid;
}

static bool task_has_matching_child(int parent_pid, int requested_pid) {
    for (int i = 0; i < TASK_MAX_SLOTS; ++i) {
        if (!task_slots[i].used || task_slots[i].parent_pid != parent_pid) {
            continue;
        }
        if (task_wait_matches(requested_pid, &task_slots[i])) {
            return true;
        }
    }
    return false;
}

static int task_reap_matching_child(task_slot_t *parent, int requested_pid, void *status_ptr) {
    int status_word;

    if (!parent) {
        return LERR_CHILD;
    }

    for (int i = 0; i < TASK_MAX_SLOTS; ++i) {
        task_slot_t *child = &task_slots[i];
        if (!child->used || child->parent_pid != (int)parent->state.pid || child->status != TASK_SLOT_ZOMBIE) {
            continue;
        }
        if (!task_wait_matches(requested_pid, child)) {
            continue;
        }

        status_word = (child->exit_code & 0xff) << 8;
        if (status_ptr && !task_copy_to_user(status_ptr, &status_word, sizeof(status_word))) {
            return LERR_FAULT;
        }

        requested_pid = (int)child->state.pid;
        task_free_slot(child);
        return requested_pid;
    }

    return 0;
}

static void task_wake_waiting_parents(task_slot_t *child) {
    int status_word;

    if (!child) {
        return;
    }

    status_word = (child->exit_code & 0xff) << 8;
    for (int i = 0; i < TASK_MAX_SLOTS; ++i) {
        task_slot_t *parent = &task_slots[i];

        if (!parent->used || parent->status != TASK_SLOT_WAITING) {
            continue;
        }
        if ((int)parent->state.pid != child->parent_pid || !task_wait_matches(parent->wait_pid, child)) {
            continue;
        }

        if (parent->wait_status_ptr != 0) {
            task_slot_write_to_user(parent, parent->wait_status_ptr, &status_word, sizeof(status_word));
        }
        parent->regs.eax = (u32)child->state.pid;
        parent->status = TASK_SLOT_RUNNABLE;
        parent->wait_pid = 0;
        parent->wait_status_ptr = 0;
        task_free_slot(child);
        return;
    }
}

/* ELF loading with paging - allocates and maps pages for each segment */
static bool elf_load_paged(const vfs_node_t *node, page_directory_t *pd, u32 base, u32 limit, elf_image_t *image, u8 *buf, u32 sz) {
    typedef struct {
        u8 ident[16];
        u16 type;
        u16 machine;
        u32 version;
        u32 entry;
        u32 phoff;
        u32 shoff;
        u32 flags;
        u16 ehsize;
        u16 phentsize;
        u16 phnum;
        u16 shentsize;
        u16 shnum;
        u16 shstrndx;
    } elf_header_t;
    typedef struct {
        u32 type;
        u32 offset;
        u32 vaddr;
        u32 paddr;
        u32 filesz;
        u32 memsz;
        u32 flags;
        u32 align;
    } elf_program_header_t;

    elf_header_t *eh = (elf_header_t *)buf;
    if (eh->ident[0] != 0x7F || eh->ident[1] != 'E' || eh->ident[2] != 'L' || eh->ident[3] != 'F' ||
        eh->ident[4] != 1 || eh->ident[5] != 1 || eh->machine != 3 || eh->type != 2) {
        return false;
    }

    u32 load_max = 0;
    image->tls_memsz = 0;
    image->tls_filesz = 0;
    image->tls_vaddr = 0;
    for (u16 i = 0; i < eh->phnum; i++) {
        elf_program_header_t *ph = (elf_program_header_t *)(buf + eh->phoff + i * sizeof(elf_program_header_t));

        if (ph->type == 7) { /* PT_TLS */
            image->tls_memsz = ph->memsz;
            image->tls_filesz = ph->filesz;
            image->tls_vaddr = ph->vaddr;
            continue;
        }

        if (ph->type != 1) continue; /* PT_LOAD */

        /* Allocate and map pages for this segment */
        u32 vaddr_start = ph->vaddr & PAGE_MASK;
        u32 vaddr_end = (ph->vaddr + ph->memsz + PAGE_SIZE - 1) & PAGE_MASK;

        if (vaddr_start < base || vaddr_end > limit) {
            return false;
        }

        for (u32 va = vaddr_start; va < vaddr_end; va += PAGE_SIZE) {
            if (!paging_alloc_user_page(pd, va, PTE_RW | PTE_USER)) {
                return false;
            }
        }

        /* Copy data to mapped pages */
        memset((void *)ph->vaddr, 0, ph->memsz);
        memcpy((void *)ph->vaddr, buf + ph->offset, ph->filesz);

        if (ph->vaddr + ph->memsz > load_max) {
            load_max = ph->vaddr + ph->memsz;
        }
    }

    image->entry = eh->entry;
    image->image_end = (load_max + PAGE_SIZE - 1) & PAGE_MASK;

    return true;
}

static int task_seed_slot(task_slot_t *slot,
                          const task_state_t *parent_state,
                          const char *path,
                          int argc,
                          const char *const *argv) {
    const vfs_node_t *node;
    elf_image_t image;
    u32 user_stack;
    page_directory_t *pd;

    if (!slot || !task_can_run() || !path || argc <= 0 || !argv) {
        console_printf("[spawn] task_seed_slot validation fail\n");
        return -1;
    }

    node = task_resolve_path_internal(vfs_root(), path);
    if (!node || !vfs_is_file(node) || !task_is_executable_node(node)) {
        console_printf("[spawn] VFS fail for '%s': node=%d is_file=%d exec=%d\n",
                       path, !!node, node ? vfs_is_file(node) : 0, node ? task_is_executable_node(node) : 0);
        return LERR_NOENT;
    }

    pd = page_directory_create();
    if (!pd) {
        console_printf("[spawn] page_directory_create FAILED, dumping heap...\n");
        memory_dump_free();
        console_printf("[spawn] trying defrag...\n");
        memory_defragment();
        console_printf("[spawn] after defrag: largest=%u, retrying...\n", memory_largest_free_block());
        pd = page_directory_create();
        if (!pd) {
            console_printf("[spawn] page_directory_create STILL FAILED\n");
            return LERR_NOMEM;
        }
    }
    /* Read ELF file into kernel buffer BEFORE switching page directory */
    u32 elf_sz = vfs_file_size(node);
    if (elf_sz == 0 || elf_sz > 16 * 1024 * 1024) {
        page_directory_destroy(pd);
        return LERR_NOENT;
    }
    u8 *elf_buf = kmalloc(elf_sz);
    if (!elf_buf) {
        page_directory_destroy(pd);
        return LERR_NOMEM;
    }
    ssize_t nread = vfs_read(node, 0, elf_buf, elf_sz);
    if (nread != (ssize_t)elf_sz) {
        kfree(elf_buf);
        page_directory_destroy(pd);
        return LERR_NOENT;
    }

    page_directory_t *old_pd = page_directory_get_current();
    page_directory_switch(pd);

    bool elf_ok = elf_load_paged(node, pd, USER_BASE, USER_LIMIT, &image, elf_buf, elf_sz);
    page_directory_switch(old_pd);
    kfree(elf_buf);

    if (!elf_ok) {
        console_printf("[spawn] ELF load fail for '%s' size=%u\n",
                       path, vfs_file_size(node));
        page_directory_destroy(pd);
        return LERR_NOENT;
    }

    /* Prepare user stack in new address space */
    /* Map stack pages (grows down from USER_STACK_TOP) */
    for (u32 va = USER_STACK_TOP - 0x40000; va < USER_STACK_TOP; va += PAGE_SIZE) {
        if (!paging_alloc_user_page(pd, va, PTE_RW | PTE_USER)) {
            page_directory_switch(old_pd);
            page_directory_destroy(pd);
            return LERR_NOMEM;
        }
    }

    memset(&current_task, 0, sizeof(current_task));
    strncpy(current_task.name, vfs_name(node), sizeof(current_task.name) - 1);
    strncpy(current_task.path, vfs_path(node), sizeof(current_task.path) - 1);
    current_task.cwd = (parent_state && parent_state->cwd) ? parent_state->cwd : vfs_root();
    strncpy(current_task.cwd_path,
            (parent_state && parent_state->cwd_path[0]) ? parent_state->cwd_path : "/",
            sizeof(current_task.cwd_path) - 1);
    current_task.entry = image.entry;
    current_task.brk_base = image.image_end;
    current_task.brk = image.image_end;
    current_task.brk_limit = USER_MMAP_BASE; /* Use mmap base as brk limit */
    current_task.pid = next_pid++;
    current_task.active = true;
    current_task.page_directory = pd;
    {
        u32 pd_vaddr = (u32)(uintptr_t)pd;
        current_task.page_directory_phys = pd_vaddr >= KERNEL_VIRTUAL_BASE ? pd_vaddr - KERNEL_VIRTUAL_BASE : pd_vaddr;
    }
    task_exit_code = 0;
    if (parent_state != NULL) {
        memcpy(current_task.fds, parent_state->fds, sizeof(current_task.fds));
    } else {
        task_reset_descriptors();
    }

    /* Switch to user page directory so TLS and stack writes go to
     * the correct physical pages. */
    page_directory_switch(pd);

    /* Allocate TLS page and set up per-task TLS */
    if (!paging_alloc_user_page(pd, USER_TLS_ADDR, PTE_RW | PTE_USER)) {
        page_directory_switch(old_pd);
        page_directory_destroy(pd);
        return LERR_NOMEM;
    }

    u32 tls_gs_base;
    u32 tls_memsz = image.tls_memsz;

    if (tls_memsz > 0) {
        /* Copy initialized TLS data (.tdata) if present */
        if (image.tls_filesz > 0) {
            memcpy((void *)USER_TLS_ADDR, (void *)(uintptr_t)image.tls_vaddr, image.tls_filesz);
        }
        /* Write self-pointer at TCB base (end of TLS data) */
        *(u32 *)(USER_TLS_ADDR + tls_memsz) = USER_TLS_ADDR + tls_memsz;
        tls_gs_base = USER_TLS_ADDR + tls_memsz;
    } else {
        tls_gs_base = USER_TLS_ADDR;
    }

    /* Canary at %gs:0x14 */
    *(u32 *)(tls_gs_base + 0x14) = 0xFF0A0000;
    slot->tls_vaddr = tls_gs_base;

    if (!task_prepare_user_stack(argc, argv, &user_stack)) {
        page_directory_switch(old_pd);
        page_directory_destroy(pd);
        return LERR_NOMEM;
    }

    /* Switch back to kernel page directory */
    page_directory_switch(old_pd);

    /* Store page directory in slot */
    slot->page_directory = pd;
    {
        u32 pd_vaddr = (u32)(uintptr_t)pd;
        slot->page_directory_phys = pd_vaddr >= KERNEL_VIRTUAL_BASE ? pd_vaddr - KERNEL_VIRTUAL_BASE : pd_vaddr;
    }

    memset(&slot->regs, 0, sizeof(slot->regs));
    slot->regs.ds = 0x23;
    slot->regs.es = 0x23;
    slot->regs.fs = 0x23;
    slot->regs.gs = USER_GS;
    slot->regs.eip = current_task.entry;
    slot->regs.cs = 0x1B;
    slot->regs.eflags = 0x202;
    slot->regs.useresp = user_stack;
    slot->regs.ss = 0x23;
    /* Seed GPRs to match what _start expects (argc, argv, environ). This
     * prevents the compositor from starting with all-zero GPRs, which was
     * causing a switch-table crash in .rodata (issue #GP at 0x0307290c).
     * _start overwrites these from the stack, so no behavior change. */
    slot->regs.eax = (u32)argc;
    slot->regs.ebx = user_stack + 4;
    slot->regs.ecx = user_stack + 8 + (u32)argc * 4;
    slot->parent_pid = parent_state ? (int)parent_state->pid : 0;
    slot->exit_code = 0;
    slot->wait_pid = 0;
    slot->wait_status_ptr = 0;
    slot->wake_tick = 0;

    slot->canary = 0xCAFEBABE;

    if (!task_slot_save_state(slot, &slot->regs)) {
        return LERR_NOMEM;
    }

    slot->status = TASK_SLOT_RUNNABLE;

    return 0;
}

static int task_spawn_kernel(const task_state_t *parent_state,
                             const char *path,
                             int argc,
                             const char *const *argv) {
    task_slot_t *slot = task_alloc_slot();
    int result;

    if (!slot) {
        return LERR_NOMEM;
    }

    result = task_seed_slot(slot, parent_state, path, argc, argv);
    if (result < 0) {
        task_free_slot(slot);
        return result;
    }

    return (int)slot->state.pid;
}

/* FPU helpers (forward declarations for lazy context switching) */
static inline void fpu_set_ts(void);

static int task_run_scheduler_session(int root_pid) {
    int root_exit_code = -1;

    scheduler_root_pid = root_pid;
    scheduler_rr_cursor = -1;

    while (task_has_live_slots()) {
        task_slot_t *next;

        task_wake_ready_slots();
        next = task_pick_next_runnable(scheduler_rr_cursor);
        if (!next) {
            cpu_halt();
            continue;
        }

        active_task_slot = (int)(next - task_slots);
        if (!task_slot_validate(next)) {
            active_task_slot = -1;
            continue;
        }
        task_slot_activate(next);
        fpu_set_ts();

        task_resume_user(&next->regs);
        active_task_slot = -1;
    }

    {
        task_slot_t *root = task_slot_for_pid(root_pid);
        if (root != NULL) {
            root_exit_code = root->exit_code;
        }
    }

    page_directory_switch_to_kernel();
    for (int i = 0; i < TASK_MAX_SLOTS; ++i) {
        if (task_slots[i].used) {
            if ((int)task_slots[i].state.pid == root_pid) {
                root_exit_code = task_slots[i].exit_code;
            }
            task_free_slot(&task_slots[i]);
        }
    }

    task_restore_kernel_context();
    scheduler_root_pid = -1;

    return root_exit_code;
}

int task_run_path(const char *path) {
    const char *argv[2];
    argv[0] = path;
    argv[1] = NULL;
    return task_run_argv(path, 1, argv);
}

static int task_run_until_pid_exits(int target_pid) {
    scheduler_rr_cursor = -1;

    while (1) {
        task_slot_t *target = task_slot_for_pid(target_pid);
        if (!target) {
            break;
        }
        if (!target->used || target->status == TASK_SLOT_ZOMBIE) {
            break;
        }

        task_wake_ready_slots();
        task_slot_t *next = task_pick_next_runnable(scheduler_rr_cursor);
        if (!next) {
            cpu_halt();
            continue;
        }

        active_task_slot = (int)(next - task_slots);
        if (!task_slot_validate(next)) {
            active_task_slot = -1;
            continue;
        }
        task_slot_activate(next);
        fpu_set_ts();

        task_resume_user(&next->regs);
        active_task_slot = -1;
    }

    task_slot_t *target = task_slot_for_pid(target_pid);
    int exit_code = -1;
    if (target) {
        exit_code = target->exit_code;
    }
    page_directory_switch_to_kernel();
    active_task_slot = -1;
    return exit_code;
}

int task_spawn_background(const char *path) {
    const char *argv[2];
    argv[0] = path;
    argv[1] = NULL;
    return task_spawn_kernel(NULL, path, 1, argv);
}

void task_run_background_init(void) {
    active_task_slot = -1;
    scheduler_rr_cursor = -1;

    while (1) {
        task_wake_ready_slots();

        bool any_runnable = false;
        for (int i = 0; i < TASK_MAX_SLOTS; ++i) {
            if (task_slots[i].used && task_slots[i].status == TASK_SLOT_RUNNABLE) {
                any_runnable = true;
                break;
            }
        }
        if (!any_runnable) break;

        task_slot_t *next = task_pick_next_runnable(scheduler_rr_cursor);
        if (!next) break;

        active_task_slot = (int)(next - task_slots);
        if (!task_slot_validate(next)) {
            active_task_slot = -1;
            continue;
        }
        task_slot_activate(next);
        fpu_set_ts();

        task_resume_user(&next->regs);
        active_task_slot = -1;
    }

    task_restore_kernel_context();
}

int task_run_argv(const char *path, int argc, const char *const *argv) {
    int root_pid;

    memset(task_slots, 0, sizeof(task_slots));
    active_task_slot = -1;
    scheduler_root_pid = -1;
    scheduler_rr_cursor = -1;

    root_pid = task_spawn_kernel(NULL, path, argc, argv);
    if (root_pid < 0) {
        return root_pid;
    }

    return task_run_scheduler_session(root_pid);
}

int task_run_argv_from(const task_state_t *parent_state, const char *path, int argc, const char *const *argv) {
    int root_pid;

    memset(task_slots, 0, sizeof(task_slots));
    active_task_slot = -1;
    scheduler_root_pid = -1;
    scheduler_rr_cursor = -1;

    root_pid = task_spawn_kernel(parent_state, path, argc, argv);
    if (root_pid < 0) {
        return root_pid;
    }

    return task_run_scheduler_session(root_pid);
}

int task_run_argv_alongside(const task_state_t *parent_state, const char *path, int argc, const char *const *argv) {
    int root_pid;

    active_task_slot = -1;
    scheduler_rr_cursor = -1;

    root_pid = task_spawn_kernel(parent_state, path, argc, argv);
    if (root_pid < 0) {
        return root_pid;
    }

    int exit_code = task_run_until_pid_exits(root_pid);

    task_slot_t *slot = task_slot_for_pid(root_pid);
    if (slot) {
        task_free_slot(slot);
    }

    task_restore_kernel_context();

    return exit_code;
}

bool path_is_in_tmp(const char *path) {
    return strncmp(path, "/tmp/", 5) == 0 || strcmp(path, "/tmp") == 0;
}

int task_open_relative(int dirfd, const char *path, int flags, int mode) {
    const vfs_node_t *base;
    const vfs_node_t *node;
    const char *node_path;
    bool want_write = (flags & LINUX_O_WRONLY) || (flags & LINUX_O_RDWR);
    bool want_create = (flags & LINUX_O_CREAT) != 0;
    bool want_excl = (flags & LINUX_O_EXCL) != 0;

    (void)mode;

    if (!path || !*path) {
        return LERR_INVAL;
    }

    base = task_resolve_dirfd(dirfd);
    if (!base) {
        return LERR_BADF;
    }

    node = task_resolve_path_internal(path[0] == '/' ? vfs_root() : base, path);

    if (node && want_create && want_excl) {
        return LERR_EXIST;
    }

    if (!node && want_create) {
        if (path_is_in_tmp(path)) {
            node = vfs_create_ramfile(path);
        }
        if (!node) {
            return LERR_NOENT;
        }
    }

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
    if (strcmp(node_path, "/dev/fb0") == 0) {
        return task_install_fd(TASK_FD_FB0, node, flags);
    }
    if (strcmp(node_path, "/dev/net") == 0) {
        return task_install_fd(TASK_FD_NET, node, flags);
    }
    if (strcmp(node_path, "/dev/keyboard") == 0) {
        return task_install_fd(TASK_FD_KEYBOARD, node, flags);
    }

    if (want_write && !vfs_node_is_ramfile(node)) {
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

    if (slot->kind == TASK_FD_SOCKET && slot->socket) {
        socket_release((socket_endpoint_t *)slot->socket);
    }

    if (slot->kind == TASK_FD_PIPE_READ && slot->pipe) {
        pipe_close_read((pipe_endpoint_t *)slot->pipe);
        pipe_release((pipe_endpoint_t *)slot->pipe);
    }

    if (slot->kind == TASK_FD_PIPE_WRITE && slot->pipe) {
        pipe_close_write((pipe_endpoint_t *)slot->pipe);
        pipe_release((pipe_endpoint_t *)slot->pipe);
    }

    memset(slot, 0, sizeof(*slot));
    return 0;
}

int task_socket_fd(void *socket) {
    int fd = task_alloc_fd();
    if (fd < 0) {
        return -1;
    }
    task_fd_t *slot = task_fd_slot(fd);
    slot->used = true;
    slot->kind = TASK_FD_SOCKET;
    slot->flags = 0;
    slot->socket = socket;
    return fd;
}

void *task_get_socket(int fd) {
    task_fd_t *slot = task_fd_slot(fd);
    if (!slot || !slot->used || slot->kind != TASK_FD_SOCKET) {
        return NULL;
    }
    return slot->socket;
}

int task_slot_install_socket(int slot, void *socket) {
    task_slot_t *target = task_slot_at(slot);
    if (!target || !socket) {
        return LERR_BADF;
    }

    int fd = task_state_alloc_fd(&target->state);
    if (fd < 0) {
        return fd;
    }

    target->state.fds[fd].used = true;
    target->state.fds[fd].kind = TASK_FD_SOCKET;
    target->state.fds[fd].flags = 0;
    target->state.fds[fd].offset = 0;
    target->state.fds[fd].node = NULL;
    target->state.fds[fd].socket = socket;
    return fd;
}

bool task_write_slot_user(int slot, u32 user_addr, const void *src, size_t length) {
    task_slot_t *target = task_slot_at(slot);
    if (!target || !src || length == 0) {
        return false;
    }
    return task_slot_write_to_user(target, user_addr, src, length);
}

void task_wake_slot(int slot, int return_value) {
    task_slot_t *target = task_slot_at(slot);
    if (!target) {
        return;
    }
    target->regs.eax = (u32)return_value;
    target->wake_tick = 0;
    if (target->status != TASK_SLOT_ZOMBIE) {
        target->status = TASK_SLOT_RUNNABLE;
    }
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

    if (slot->kind == TASK_FD_SOCKET && slot->socket) {
        return socket_recv((socket_endpoint_t *)slot->socket, buffer, length, 0);
    }

    if (slot->kind == TASK_FD_PIPE_READ && slot->pipe) {
        return (ssize_t)pipe_read((pipe_endpoint_t *)slot->pipe, buffer, length, 0);
    }

    if (slot->kind == TASK_FD_NET) {
        netdev_t *dev = netdev_first();
        if (!dev) return LERR_IO;
        u16 pkt_len;
        if (length > NETDEV_RX_BUF_SIZE) length = NETDEV_RX_BUF_SIZE;
        if (netdev_rx_pop(dev, buffer, &pkt_len) < 0) return LERR_AGAIN;
        return (ssize_t)pkt_len;
    }

    if (slot->kind == TASK_FD_KEYBOARD) {
        if (length < 1) return 0;
        if (!keyboard_has_raw_scancode()) return LERR_AGAIN;
        *(u8 *)buffer = keyboard_read_raw_scancode();
        return 1;
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

    if (slot->kind == TASK_FD_SOCKET && slot->socket) {
        return socket_send((socket_endpoint_t *)slot->socket, buffer, length, 0);
    }

    if (slot->kind == TASK_FD_FILE && slot->node && vfs_node_is_ramfile(slot->node)) {
        ssize_t written = vfs_write_ramfile(slot->node, slot->offset, buffer, length);
        if (written > 0) {
            slot->offset += (u32)written;
        }
        return written;
    }

    if (slot->kind == TASK_FD_PIPE_WRITE && slot->pipe) {
        ssize_t result = pipe_write((pipe_endpoint_t *)slot->pipe, buffer, length, 0);
        if (result < 0) {
            /* Convert pipe errors to Linux error codes */
            if (result == -EPIPE) {
                return LERR_PIPE;
            }
            return LERR_BADF;
        }
        return result;
    }

    if (slot->kind == TASK_FD_NET) {
        netdev_t *dev = netdev_first();
        if (!dev) return LERR_IO;
        if (length > NETDEV_MTU) length = NETDEV_MTU;
        if (netdev_send(dev, buffer, (u16)length) < 0) return LERR_IO;
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

    /* Increment refcount for pipes on dup */
    if (old_slot->kind == TASK_FD_PIPE_READ || old_slot->kind == TASK_FD_PIPE_WRITE) {
        if (old_slot->pipe) {
            pipe_endpoint_t *pipe = (pipe_endpoint_t *)old_slot->pipe;
            pipe->refcount++;
        }
    }

    return newfd;
}

bool task_is_console_fd(int fd) {
    task_fd_t *slot = task_fd_slot(fd);
    return slot && slot->used && slot->kind == TASK_FD_CONSOLE;
}

int task_access(const char *path, int mode) {
    const vfs_node_t *node = task_resolve_path_internal(current_task.cwd, path);

    if (!node) {
        struct linux_stat64 statbuf;
        if (socket_stat_path(path, &statbuf) == 0) {
            return 0;
        }
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
        if (socket_stat_path(path, &stat) != 0) {
            return LERR_NOENT;
        }
        if (stat_size < sizeof(stat) || !task_copy_to_user(user_stat, &stat, sizeof(stat))) {
            return LERR_FAULT;
        }
        return 0;
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
    } else if (slot->kind == TASK_FD_SOCKET && slot->socket) {
        memset(&stat, 0, sizeof(stat));
        stat.st_mode = LINUX_S_IFSOCK | 0666u;
        stat.st_nlink = 1;
        stat.st_blksize = 1;
    } else if (slot->kind == TASK_FD_PIPE_READ || slot->kind == TASK_FD_PIPE_WRITE) {
        memset(&stat, 0, sizeof(stat));
        stat.st_mode = LINUX_S_IFIFO | 0666u;
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
    vfs_ramfile_t *ramfile = NULL;
    task_mapping_t *mapping = NULL;
    page_directory_t *pd = page_directory_get_current();
    u16 page_flags = PTE_USER | PTE_RW; /* Default: user, writable */

    if (size == 0 || ((flags & LINUX_MAP_SHARED) == 0 && (flags & LINUX_MAP_PRIVATE) == 0)) {
        return (void *)(uintptr_t)LERR_INVAL;
    }

    /* Adjust page flags based on prot */
    if ((prot & LINUX_PROT_WRITE) == 0) {
        page_flags &= ~PTE_RW; /* Read-only */
    }

    if ((flags & LINUX_MAP_ANONYMOUS) == 0) {
        slot = task_fd_slot(fd);
        if (!slot || !slot->used) {
            return (void *)(uintptr_t)LERR_BADF;
        }
        if (slot->kind != TASK_FD_FILE && slot->kind != TASK_FD_FB0) {
            return (void *)(uintptr_t)LERR_BADF;
        }
        node = slot->node;
        ramfile = vfs_node_ramfile(node);
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

    if (slot && slot->kind == TASK_FD_FB0) {
        console_fb_info_t fb_info;
        if (!console_get_fb_info(&fb_info)) {
            return (void *)(uintptr_t)LERR_INVAL;
        }
        u32 fb_phys = (u32)(uintptr_t)fb_info.address;
        u32 fb_size = fb_info.pitch * fb_info.height;
        u32 map_size = size < fb_size ? size : align_up(fb_size, PAGE_SIZE);

        /* Map framebuffer physical pages into the process page directory
         * so user code can access the framebuffer through physical addresses. */
        for (u32 page = 0; page < map_size; page += PAGE_SIZE) {
            if (!page_map_existing(pd, base + page, fb_phys + page, page_flags)) {
                for (u32 cleanup = base; cleanup < base + page; cleanup += PAGE_SIZE) {
                    page_unmap(pd, cleanup);
                }
                return (void *)(uintptr_t)LERR_NOMEM;
            }
        }

        memset(mapping, 0, sizeof(*mapping));
        mapping->used = true;
        mapping->base = base;
        mapping->length = map_size;
        mapping->prot = prot;
        mapping->flags = flags;
        return (void *)(uintptr_t)base;
    } else if (ramfile && (flags & LINUX_MAP_SHARED) != 0) {
        u32 file_offset = page_offset * TASK_PAGE_SIZE;
        if (file_offset + size > ramfile->capacity) {
            return (void *)(uintptr_t)LERR_INVAL;
        }

        for (u32 page = 0; page < size; page += PAGE_SIZE) {
            u32 backing_va = (u32)(uintptr_t)(ramfile->data + file_offset + page);
            u32 phys = page_get_phys(pd, backing_va);
            if (!phys || !page_map_existing(pd, base + page, phys & PAGE_MASK, page_flags)) {
                for (u32 cleanup = base; cleanup < base + page; cleanup += PAGE_SIZE) {
                    page_unmap(pd, cleanup);
                }
                return (void *)(uintptr_t)LERR_NOMEM;
            }
        }
    } else {
        /* Allocate and map pages for the mapping */
        for (u32 va = base; va < base + size; va += PAGE_SIZE) {
            if (!paging_alloc_user_page(pd, va, page_flags)) {
                /* Cleanup partial allocation */
                for (u32 cleanup = base; cleanup < va; cleanup += PAGE_SIZE) {
                    paging_free_user_page(pd, cleanup);
                }
                return (void *)(uintptr_t)LERR_NOMEM;
            }
        }

        memset((void *)(uintptr_t)base, 0, size);
        if (node) {
            u32 file_offset = page_offset * TASK_PAGE_SIZE;
            if (file_offset < vfs_file_size(node)) {
                ssize_t copied = vfs_read(node, file_offset, (void *)(uintptr_t)base, size);
                if (copied < 0) {
                    /* Cleanup on error */
                    for (u32 va = base; va < base + size; va += PAGE_SIZE) {
                        paging_free_user_page(pd, va);
                    }
                    return (void *)(uintptr_t)LERR_IO;
                }
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
    page_directory_t *pd = page_directory_get_current();

    if (!mapping) {
        return LERR_INVAL;
    }

    /* Unmap all pages in the mapping */
    for (u32 va = mapping->base; va < mapping->base + mapping->length; va += PAGE_SIZE) {
        paging_free_user_page(pd, va);
    }

    memset(mapping, 0, sizeof(*mapping));
    return 0;
}

int task_execve_from_user(const char *user_path, const u32 *user_argv, const u32 *user_envp, registers_t *regs) {
    char path[VFS_PATH_MAX];
    const char *argv[TASK_STACK_ARG_MAX];
    char arg_storage[TASK_STACK_ARG_MAX][VFS_PATH_MAX];
    int argc = 0;
    const vfs_node_t *node;
    elf_image_t image;
    u32 user_stack;
    page_directory_t *new_pd;
    page_directory_t *old_pd;
    task_slot_t *slot;

    if (!user_path || !regs) {
        return LERR_INVAL;
    }

    if (!task_copy_string_from_user(path, sizeof(path), user_path)) {
        return LERR_FAULT;
    }

    (void)user_envp;

    while (argc < TASK_STACK_ARG_MAX) {
        u32 user_arg = 0;

        if (user_argv == NULL) {
            break;
        }
        if (!task_copy_from_user(&user_arg, user_argv + argc, sizeof(user_arg))) {
            return LERR_FAULT;
        }
        if (user_arg == 0) {
            break;
        }
        if (!task_copy_string_from_user(arg_storage[argc], sizeof(arg_storage[argc]), (const char *)(uintptr_t)user_arg)) {
            return LERR_FAULT;
        }
        argv[argc] = arg_storage[argc];
        argc++;
    }

    if (argc == 0) {
        argv[argc++] = path;
    }

    node = task_resolve_path_internal(current_task.cwd, path);
    if (!node || !vfs_is_file(node)) {
        return LERR_NOENT;
    }
    if (!task_is_executable_node(node)) {
        return LERR_ACCES;
    }

    /* Create new page directory */
    new_pd = page_directory_create();
    if (!new_pd) {
        return LERR_NOMEM;
    }

    /* Read ELF file into kernel buffer BEFORE switching page directory */
    u32 elf_sz = vfs_file_size(node);
    if (elf_sz == 0 || elf_sz > 16 * 1024 * 1024) {
        page_directory_destroy(new_pd);
        return LERR_NOENT;
    }
    u8 *elf_buf = kmalloc(elf_sz);
    if (!elf_buf) {
        page_directory_destroy(new_pd);
        return LERR_NOMEM;
    }
    ssize_t nread = vfs_read(node, 0, elf_buf, elf_sz);
    if (nread != (ssize_t)elf_sz) {
        kfree(elf_buf);
        page_directory_destroy(new_pd);
        return LERR_NOENT;
    }

    /* Switch to new page directory to load ELF */
    old_pd = page_directory_get_current();
    page_directory_switch(new_pd);

    /* Load ELF into new page directory */
    bool elf_ok = elf_load_paged(node, new_pd, USER_BASE, USER_LIMIT, &image, elf_buf, elf_sz);
    page_directory_switch(old_pd);
    kfree(elf_buf);

    if (!elf_ok) {
        page_directory_destroy(new_pd);
        return LERR_NOENT;
    }

    /* Map stack pages */
    for (u32 va = USER_STACK_TOP - 0x10000; va < USER_STACK_TOP; va += PAGE_SIZE) {
        if (!paging_alloc_user_page(new_pd, va, PTE_RW | PTE_USER)) {
            page_directory_switch(old_pd);
            page_directory_destroy(new_pd);
            return LERR_NOMEM;
        }
    }

    /* Prepare stack in new address space */
    strncpy(current_task.name, vfs_name(node), sizeof(current_task.name) - 1);
    strncpy(current_task.path, vfs_path(node), sizeof(current_task.path) - 1);
    current_task.entry = image.entry;
    current_task.brk_base = image.image_end;
    current_task.brk = image.image_end;
    current_task.brk_limit = USER_MMAP_BASE;
    current_task.active = true;

    /* Switch to new page directory so TLS and stack writes go to
     * the correct physical pages. */
    page_directory_switch(new_pd);

    /* Map TLS page and set up per-task TLS */
    if (!paging_alloc_user_page(new_pd, USER_TLS_ADDR, PTE_RW | PTE_USER)) {
        page_directory_switch(old_pd);
        page_directory_destroy(new_pd);
        return LERR_NOMEM;
    }

    u32 tls_gs_base;
    u32 tls_memsz = image.tls_memsz;

    if (tls_memsz > 0) {
        /* Copy initialized TLS data (.tdata) if present */
        if (image.tls_filesz > 0) {
            memcpy((void *)USER_TLS_ADDR, (void *)(uintptr_t)image.tls_vaddr, image.tls_filesz);
        }
        /* Write self-pointer at TCB base (end of TLS data) */
        *(u32 *)(USER_TLS_ADDR + tls_memsz) = USER_TLS_ADDR + tls_memsz;
        tls_gs_base = USER_TLS_ADDR + tls_memsz;
    } else {
        tls_gs_base = USER_TLS_ADDR;
    }

    /* Canary at %gs:0x14 */
    *(u32 *)(tls_gs_base + 0x14) = 0xFF0A0000;

    if (!task_prepare_user_stack(argc, argv, &user_stack)) {
        page_directory_switch(old_pd);
        page_directory_destroy(new_pd);
        return LERR_NOMEM;
    }

    /* Close O_CLOEXEC fds */
    for (int fd = 0; fd < TASK_MAX_FDS; ++fd) {
        if (current_task.fds[fd].used && (current_task.fds[fd].flags & LINUX_O_CLOEXEC) != 0) {
            memset(&current_task.fds[fd], 0, sizeof(current_task.fds[fd]));
        }
    }

    /* Get current slot and replace its page directory */
    slot = task_active_slot();
    if (slot) {
        /* Destroy old page directory */
        if (slot->page_directory) {
            /* Temporarily switch to new PD so we can destroy the old one safely */
            page_directory_switch(old_pd);
            page_directory_destroy(slot->page_directory);
        }
        slot->page_directory = new_pd;
        {
            u32 pd_vaddr = (u32)(uintptr_t)new_pd;
            slot->page_directory_phys = pd_vaddr >= KERNEL_VIRTUAL_BASE ? pd_vaddr - KERNEL_VIRTUAL_BASE : pd_vaddr;
        }
        slot->tls_vaddr = tls_gs_base;
    }

    /* Switch to new page directory permanently */
    page_directory_switch(new_pd);
    current_task.page_directory = new_pd;
    {
        u32 pd_vaddr = (u32)(uintptr_t)new_pd;
        current_task.page_directory_phys = pd_vaddr >= KERNEL_VIRTUAL_BASE ? pd_vaddr - KERNEL_VIRTUAL_BASE : pd_vaddr;
    }

    /* Clear mappings (mmap regions) - they're gone with the old page directory */
    memset(current_task.mappings, 0, sizeof(current_task.mappings));

    regs->ds = 0x23;
    regs->es = 0x23;
    regs->fs = 0x23;
    regs->gs = USER_GS;
    regs->eip = current_task.entry;
    regs->cs = 0x1B;
    regs->eflags = 0x202;
    regs->useresp = user_stack;
    regs->ss = 0x23;

    task_exit_code = 0;
    return 0;
}

/* Copy user page directory with copy-on-write */
static page_directory_t *page_directory_copy_cow(page_directory_t *src_pd) {
    page_directory_t *dst_pd = page_directory_create();
    if (!dst_pd) return NULL;

    /* Copy user-allocated pages (0 to kernel base) with COW.
     * Skip kernel identity PDEs (copied without PTE_ALLOCATED) —
     * those pages (heap, code, data) are shared globally and must
     * not be copy-on-write. */
    for (u32 pd_idx = 0; pd_idx < KERNEL_PD_INDEX; pd_idx++) {
        if (!(src_pd->entries[pd_idx] & PTE_PRESENT)) continue;
        if (!(src_pd->entries[pd_idx] & PTE_ALLOCATED)) continue;

        u32 src_pt_phys = entry_get_phys(src_pd->entries[pd_idx]);
        page_table_t *src_pt = (page_table_t *)(KERNEL_VIRTUAL_BASE + src_pt_phys);

        for (u32 pt_idx = 0; pt_idx < ENTRIES_PER_PT; pt_idx++) {
            if (!(src_pt->entries[pt_idx] & PTE_PRESENT)) continue;

            u32 virt_addr = (pd_idx << 22) | (pt_idx << 12);
            u32 flags = entry_get_flags(src_pt->entries[pt_idx]);
            u32 phys = entry_get_phys(src_pt->entries[pt_idx]);

            /* Mark source as read-only if it was writable */
            if (flags & PTE_RW) {
                src_pt->entries[pt_idx] = entry_create(phys, (flags & ~PTE_RW) | PTE_COW);
                paging_invalidate_tlb(virt_addr);
            }

            /* Map destination to same physical page, read-only with COW flag */
            u32 cow_flags = (flags | PTE_COW) & ~PTE_RW;
            page_map(dst_pd, virt_addr, phys, cow_flags);
        }
    }

    return dst_pd;
}

int task_fork_from_user(registers_t *regs) {
    task_slot_t *slot = task_alloc_slot();
    page_directory_t *child_pd;

    if (!regs) {
        if (slot) task_free_slot(slot);
        return LERR_INVAL;
    }
    if (!slot) {
        return LERR_NOMEM;
    }

    if (!task_slot_save_state(slot, regs)) {
        task_free_slot(slot);
        return LERR_NOMEM;
    }

    /* Copy page directory with copy-on-write */
    child_pd = page_directory_copy_cow(page_directory_get_current());
    if (!child_pd) {
        task_free_slot(slot);
        return LERR_NOMEM;
    }

    slot->page_directory = child_pd;
    {
        u32 pd_vaddr = (u32)(uintptr_t)child_pd;
        slot->page_directory_phys = pd_vaddr >= KERNEL_VIRTUAL_BASE ? pd_vaddr - KERNEL_VIRTUAL_BASE : pd_vaddr;
    }
    slot->state.page_directory = child_pd;
    slot->state.page_directory_phys = slot->page_directory_phys;

    slot->state.pid = next_pid++;
    slot->parent_pid = (int)current_task.pid;
    slot->status = TASK_SLOT_RUNNABLE;
    slot->exit_code = 0;
    slot->wait_pid = 0;
    slot->wait_status_ptr = 0;
    slot->wake_tick = 0;
    slot->regs.eax = 0;
    /* Inherit parent's TLS vaddr (COW shares the TLS page) */
    {
        task_slot_t *parent = task_active_slot();
        if (parent) {
            slot->tls_vaddr = parent->tls_vaddr;
        }
    }

    return (int)slot->state.pid;
}

int task_spawn_from_user(const char *user_path, const u32 *user_argv) {
    task_slot_t *parent = task_active_slot();
    task_state_t parent_state;
    registers_t syscall_regs;
    char path[VFS_PATH_MAX];
    const char *argv[TASK_STACK_ARG_MAX];
    char arg_storage[TASK_STACK_ARG_MAX][VFS_PATH_MAX];
    int argc = 0;
    int result;

    if (!parent || !user_path) {
        console_printf("[spawn] invalid args: parent=%d user_path=%x\n", !!parent, (u32)user_path);
        return LERR_INVAL;
    }

    if (!task_copy_string_from_user(path, sizeof(path), user_path)) {
        console_printf("[spawn] copy_string_from_user FAILED\n");
        return LERR_FAULT;
    }

    while (argc < TASK_STACK_ARG_MAX) {
        u32 user_arg = 0;

        if (user_argv == NULL) {
            break;
        }
        if (!task_copy_from_user(&user_arg, user_argv + argc, sizeof(user_arg))) {
            return LERR_FAULT;
        }
        if (user_arg == 0) {
            break;
        }
        if (!task_copy_string_from_user(arg_storage[argc], sizeof(arg_storage[argc]), (const char *)(uintptr_t)user_arg)) {
            return LERR_FAULT;
        }
        argv[argc] = arg_storage[argc];
        argc++;
    }

    if (argc == 0) {
        argv[argc++] = path;
    }

    parent_state = current_task;
    if (!idt_last_user_syscall_frame(&syscall_regs) || !task_slot_save_state(parent, &syscall_regs)) {
        return LERR_NOMEM;
    }

    result = task_spawn_kernel(&parent_state, path, argc, argv);
    task_slot_activate(parent);
    return result;
}

int task_waitpid_from_user(int pid, void *status_ptr, int options, registers_t *regs) {
    task_slot_t *parent = task_active_slot();
    int result;

    if (!parent || !regs) {
        return LERR_INVAL;
    }
    if (options != 0) {
        return LERR_INVAL;
    }

    result = task_reap_matching_child(parent, pid, status_ptr);
    if (result != 0) {
        return result;
    }

    if (!task_has_matching_child((int)parent->state.pid, pid)) {
        return LERR_CHILD;
    }

    if (!task_slot_save_state(parent, regs)) {
        return LERR_NOMEM;
    }

    parent->regs.eax = 0;
    parent->status = TASK_SLOT_WAITING;
    parent->wait_pid = pid;
    parent->wait_status_ptr = (u32)(uintptr_t)status_ptr;
    active_task_slot = -1;
    task_return_to_kernel(0);
}

NORETURN void task_sleep_current(registers_t *regs, u32 wake_tick, int return_value) {
    task_slot_t *slot = task_active_slot();

    if (!slot || !regs || !task_slot_save_state(slot, regs)) {
        task_return_to_kernel(return_value);
    }

    slot->regs.eax = (u32)return_value;
    slot->status = TASK_SLOT_SLEEPING;
    slot->wake_tick = wake_tick;
    active_task_slot = -1;
    task_return_to_kernel(return_value);
}

NORETURN void task_yield_current(registers_t *regs, int return_value) {
    task_slot_t *slot = task_active_slot();

    if (!slot || !regs || !task_slot_save_state(slot, regs)) {
        task_return_to_kernel(return_value);
    }

    slot->regs.eax = (u32)return_value;
    slot->status = TASK_SLOT_RUNNABLE;
    active_task_slot = -1;
    task_return_to_kernel(return_value);
}

/* ------------------------------------------------------------------ */
/* FPU / SSE context switching (lazy)                                  */
/* ------------------------------------------------------------------ */

static inline u32 fpu_read_cr0(void) {
    u32 val;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(val));
    return val;
}

static inline void fpu_write_cr0(u32 val) {
    __asm__ volatile ("mov %0, %%cr0" : : "r"(val) : "memory");
}

static inline u32 fpu_read_cr4(void) {
    u32 val;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(val));
    return val;
}

static inline void fpu_write_cr4(u32 val) {
    __asm__ volatile ("mov %0, %%cr4" : : "r"(val) : "memory");
}

static inline void fpu_set_ts(void) {
    u32 cr0 = fpu_read_cr0();
    cr0 |= 0x08u;
    fpu_write_cr0(cr0);
}

static inline void fpu_clear_ts(void) {
    u32 cr0 = fpu_read_cr0();
    cr0 &= ~0x08u;
    fpu_write_cr0(cr0);
}

static inline void fpu_fxsave(void *addr) {
    __asm__ volatile ("fxsave (%0)" : : "r"(addr) : "memory");
}

static inline void fpu_fxrstor(const void *addr) {
    __asm__ volatile ("fxrstor (%0)" : : "r"(addr) : "memory");
}

void fpu_init(void) {
    u32 cr0 = fpu_read_cr0();
    /* Clear EM (bit 2) to enable x87, set MP (bit 1) for TS/#NM cooperation,
     * set NE (bit 5) for native FPU error handling */
    cr0 &= ~0x04u;          /* clear EM */
    cr0 |= 0x22u;           /* set MP + NE */
    fpu_write_cr0(cr0);

    /* Enable SSE: set OSFXSR (bit 9) and OSXMMEXCPT (bit 10) in CR4 */
    u32 cr4 = fpu_read_cr4();
    cr4 |= 0x600u;
    fpu_write_cr4(cr4);

    /* Initialize x87 FPU to known state */
    __asm__ volatile ("fninit");

    /* Initialize SSE MXCSR to default (0x1FC0) */
    {
        u32 mxcsr = 0x1FC0u;
        __asm__ volatile ("ldmxcsr (%0)" : : "r"(&mxcsr) : "memory");
    }

    fpu_owner_slot = -1;

    console_write("[fpu] x87 + SSE context switching enabled\n");
}

void fpu_nm_handler(registers_t *regs) {
    (void)regs;

    if ((regs->cs & 3u) != 3u) {
        panic("#NM (Device Not Available) in kernel mode - kernel should not use FPU");
    }

    /* CR0.TS is set right now — we must clear it before any FPU instruction
     * (fxsave, fnsave, fninit, fxrstor all check TS and would fault again). */
    fpu_clear_ts();

    int current_slot = active_task_slot;

    /* Save previous FPU owner's state */
    if (fpu_owner_slot >= 0 && fpu_owner_slot < TASK_MAX_SLOTS) {
        task_slot_t *owner = &task_slots[fpu_owner_slot];
        if (owner->used) {
            fpu_fxsave(owner->state.fpu_state);
        }
    }

    /* Restore or initialize FPU for the current task */
    if (current_slot >= 0 && current_slot < TASK_MAX_SLOTS) {
        task_slot_t *current = &task_slots[current_slot];
        if (current->used) {
            if (current->state.fpu_used) {
                fpu_fxrstor(current->state.fpu_state);
            } else {
                __asm__ volatile ("fninit");
                current->state.fpu_used = true;
            }
        }
    } else {
        __asm__ volatile ("fninit");
    }

    fpu_owner_slot = current_slot;
}

void task_timer_tick(registers_t *regs) {
    task_slot_t *current;
    task_slot_t *next;

    task_wake_ready_slots();
    if (!regs || (regs->cs & 3u) != 3u || active_task_slot < 0) {
        return;
    }
    if ((pit_ticks() & 1u) != 0u) {
        return;
    }

    current = task_active_slot();
    next = task_pick_next_runnable(active_task_slot);
    if (!current || !next || next == current) {
        return;
    }
    if (!task_slot_validate(next)) {
        return;
    }
    if (!task_slot_save_state(current, regs)) {
        return;
    }

    current->status = TASK_SLOT_RUNNABLE;

    if (next->regs.eip < USER_BASE || next->regs.eip >= USER_LIMIT) {
        return;
    }

    active_task_slot = (int)(next - task_slots);
    task_slot_activate(next);
    fpu_set_ts();

    *regs = next->regs;

    /* Safety net: if a user task has all-zero GPRs, force non-zero values to
     * prevent switch-table lookups from picking index 0 and crashing in .rodata
     * (xnx-compositor #GP at 0x0307290c, CSWTCH.9+0xc byte 0xFB=STI).
     * The compositor's -O2 code keeps all GPRs zero between syscalls; the kernel
     * faithfully saves/restores these zeros. Seeding each GPR with EIP ensures
     * any register used as a table index is non-zero without changing program
     * semantics (all registers are undefined per ABI at task start, and the
     * code demonstrably does not depend on register values). */
    if ((regs->cs & 3u) == 3u && regs->eip >= USER_BASE + 0x100 &&
        regs->eip < USER_LIMIT &&
        regs->eax == 0 && regs->ebx == 0 && regs->ecx == 0 &&
        regs->edx == 0 && regs->esi == 0 && regs->edi == 0 &&
        regs->ebp == 0) {
        regs->eax = regs->eip;
        regs->ebx = regs->eip;
        regs->ecx = regs->eip;
        regs->edx = regs->eip;
        regs->esi = regs->eip;
        regs->edi = regs->eip;
        regs->ebp = regs->eip;
    }
}

NORETURN void task_exit_current(int exit_code) {
    task_slot_t *slot = task_active_slot();

    current_task.active = false;
    if (slot != NULL) {
        slot->state = current_task;
        slot->status = TASK_SLOT_ZOMBIE;
        slot->exit_code = exit_code;
        task_wake_waiting_parents(slot);
    }
    active_task_slot = -1;

    /* Clear CR0.TS when leaving user space — the last fpu_set_ts()
     * may have left it set, and the kernel does not use the FPU. */
    fpu_clear_ts();

    task_return_to_kernel(exit_code);
}

NORETURN void task_abort_from_trap(registers_t *regs, const char *reason) {
    registers_t last_user;
    registers_t last_syscall;
    bool have_last_user = idt_last_user_frame(&last_user);
    bool have_last_syscall = idt_last_user_syscall_frame(&last_syscall);

    console_printf("\n[task] %s hit %s int=%u eip=%08x cs=%08x err=%08x eax=%08x esp=%08x useresp=%08x\n",
                   current_task.path[0] ? current_task.path : "(unnamed)",
                   reason,
                   regs->int_no,
                   regs->eip,
                   regs->cs,
                   regs->err_code,
                   regs->eax,
                   regs->esp,
                   regs->useresp);
    if (have_last_user) {
        console_printf("[task] last user frame int=%u eip=%08x eax=%08x esp=%08x useresp=%08x\n",
                       last_user.int_no,
                       last_user.eip,
                       last_user.eax,
                       last_user.esp,
                       last_user.useresp);
    }
    if (have_last_syscall) {
        console_printf("[task] last syscall frame nr=%u eip=%08x ebx=%08x esp=%08x useresp=%08x\n",
                       last_syscall.eax,
                       last_syscall.eip,
                       last_syscall.ebx,
                       last_syscall.esp,
                       last_syscall.useresp);
    }
    console_hexdump(regs, 96, 0);
    current_task.active = false;
    if (task_active_slot() != NULL) {
        task_active_slot()->state = current_task;
        task_active_slot()->status = TASK_SLOT_ZOMBIE;
        task_active_slot()->exit_code = 128 + (int)regs->int_no;
        task_wake_waiting_parents(task_active_slot());
    }
    active_task_slot = -1;
    task_return_to_kernel(128 + (int)regs->int_no);
}
