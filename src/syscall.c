#include "syscall.h"

#include "abi/linux.h"
#include "console.h"
#include "io.h"
#include "keyboard.h"
#include "memory.h"
#include "netdev.h"
#include "pipe.h"
#include "pit.h"
#include "socket.h"
#include "string.h"
#include "task.h"
#include "vfs.h"
#include "memory.h"
#include "pit.h"
#include "power.h"

extern task_state_t current_task;

#define SYSCALL_PATH_MAX 256
#define SYSCALL_IOV_MAX  16

#define LERR_PERM    (-1)
#define LERR_NOENT   (-2)
#define LERR_IO      (-5)
#define LERR_INTR    (-4)
#define LERR_BADF    (-9)
#define LERR_FAULT   (-14)
#define LERR_NOTTY   (-25)
#define LERR_INVAL   (-22)
#define LERR_EXIST   (-17)
#define LERR_ROFS    (-30)
#define LERR_NFILE   (-23)
#define LERR_PIPE    (-32)

static u32 divide_u64_u32(u64 dividend, u32 divisor) {
    u64 remainder = 0;
    u32 quotient = 0;

    if (divisor == 0) {
        return 0;
    }

    for (int bit = 63; bit >= 0; --bit) {
        remainder = (remainder << 1) | ((dividend >> bit) & 1ull);
        if (remainder >= divisor) {
            remainder -= divisor;
            if (bit < 32) {
                quotient |= (1u << bit);
            }
        }
    }

    return quotient;
}

static u32 uptime_seconds(void) {
    u32 hz = pit_frequency();
    return hz ? (pit_ticks() / hz) : 0;
}

static u32 uptime_nanoseconds(void) {
    u32 hz = pit_frequency();
    u32 ticks = pit_ticks();
    u32 whole = hz ? (ticks / hz) : 0;
    u32 rem = hz ? (ticks - whole * hz) : 0;
    return hz ? divide_u64_u32((u64)rem * 1000000000ull, hz) : 0;
}

static int sys_exit(registers_t *regs) {
    task_exit_current((int)regs->ebx);
}

static int sys_exit_group(registers_t *regs) {
    task_exit_current((int)regs->ebx);
}

static int sys_fork(registers_t *regs) {
    return task_fork_from_user(regs);
}

static int sys_execve(registers_t *regs) {
    return task_execve_from_user((const char *)(uintptr_t)regs->ebx,
                                 (const u32 *)(uintptr_t)regs->ecx,
                                 (const u32 *)(uintptr_t)regs->edx,
                                 regs);
}

static int sys_read(registers_t *regs) {
    int fd = (int)regs->ebx;
    void *buffer = (void *)(uintptr_t)regs->ecx;
    u32 length = regs->edx;
    ssize_t result;

    if (length == 0) {
        return 0;
    }
    if (!task_validate_user_range((uintptr_t)buffer, length)) {
        return LERR_FAULT;
    }

    interrupts_enable();
    result = task_read_fd(fd, buffer, length);
    interrupts_disable();
    return (int)result;
}

static int sys_write(registers_t *regs) {
    int fd = (int)regs->ebx;
    const void *buffer = (const void *)(uintptr_t)regs->ecx;
    u32 length = regs->edx;

    if (!task_validate_user_range((uintptr_t)buffer, length)) {
        return LERR_FAULT;
    }

    return (int)task_write_fd(fd, buffer, length);
}

static int sys_open_like(int dirfd, registers_t *regs) {
    char path[SYSCALL_PATH_MAX];

    if (!task_copy_string_from_user(path, sizeof(path), (const char *)(uintptr_t)regs->ebx)) {
        return LERR_FAULT;
    }

    if (dirfd == LINUX_AT_FDCWD) {
        return task_open(path, (int)regs->ecx, (int)regs->edx);
    }
    return task_open_relative(dirfd, path, (int)regs->ecx, (int)regs->edx);
}

static int sys_open(registers_t *regs) {
    return sys_open_like(LINUX_AT_FDCWD, regs);
}

static int sys_close(registers_t *regs) {
    return task_close((int)regs->ebx);
}

static int sys_lseek(registers_t *regs) {
    return task_lseek((int)regs->ebx, (i32)regs->ecx, (int)regs->edx);
}

static int sys_getpid(UNUSED registers_t *regs) {
    const task_state_t *state = task_state();
    return (int)state->pid;
}

static int sys_access(registers_t *regs) {
    char path[SYSCALL_PATH_MAX];

    if (!task_copy_string_from_user(path, sizeof(path), (const char *)(uintptr_t)regs->ebx)) {
        return LERR_FAULT;
    }
    return task_access(path, (int)regs->ecx);
}

static int sys_ioctl(registers_t *regs) {
    int fd = (int)regs->ebx;
    u32 request = regs->ecx;
    void *argp = (void *)(uintptr_t)regs->edx;

    if (request == LINUX_TCGETS) {
        struct linux_termios termios;
        memset(&termios, 0, sizeof(termios));
        termios.c_oflag = 0x00004u;
        termios.c_cflag = 0x000000B0u;
        termios.c_lflag = 0x0000000Bu;
        termios.c_cc[2] = 0x08;
        termios.c_cc[3] = 0x15;
        termios.c_cc[4] = 0x04;
        termios.c_cc[5] = 0;
        termios.c_cc[6] = 1;
        if (!task_is_console_fd(fd) || !task_copy_to_user(argp, &termios, sizeof(termios))) {
            return LERR_FAULT;
        }
        return 0;
    }

    if (request == LINUX_TIOCGWINSZ) {
        struct linux_winsize winsize;
        size_t cols = 80, rows = 25;
        console_get_dimensions(&cols, &rows);
        winsize.ws_row = (u16)rows;
        winsize.ws_col = (u16)cols;
        winsize.ws_xpixel = 0;
        winsize.ws_ypixel = 0;
        if (!task_is_console_fd(fd) || !task_copy_to_user(argp, &winsize, sizeof(winsize))) {
            return LERR_FAULT;
        }
        return 0;
    }

    if (request == LINUX_FIONREAD) {
        task_fd_t *slot = task_fd_slot(fd);
        int value = 0;

        if (!slot || !slot->used || !argp) {
            return LERR_FAULT;
        }
        if (slot->kind == TASK_FD_SOCKET && slot->socket) {
            value = socket_pending_readable((socket_endpoint_t *)slot->socket);
            if (value < 0) {
                return value;
            }
            if (!task_copy_to_user(argp, &value, sizeof(value))) {
                return LERR_FAULT;
            }
            return 0;
        }
    }

    if (request == FBIOGET_VSCREENINFO) {
        task_fd_t *slot = task_fd_slot(fd);
        if (!slot || !slot->used || slot->kind != TASK_FD_FB0) {
            return LERR_FAULT;
        }
        console_fb_info_t fb;
        if (!console_get_fb_info(&fb)) {
            return LERR_INVAL;
        }
        struct fb_var_screeninfo vinfo;
        memset(&vinfo, 0, sizeof(vinfo));
        vinfo.xres = fb.width;
        vinfo.yres = fb.height;
        vinfo.xres_virtual = fb.width;
        vinfo.yres_virtual = fb.height;
        vinfo.bits_per_pixel = fb.bpp;
        vinfo.pitch = fb.pitch;
        vinfo.red.offset = fb.red_shift;
        vinfo.red.length = fb.red_mask_size;
        vinfo.green.offset = fb.green_shift;
        vinfo.green.length = fb.green_mask_size;
        vinfo.blue.offset = fb.blue_shift;
        vinfo.blue.length = fb.blue_mask_size;
        if (!task_copy_to_user(argp, &vinfo, sizeof(vinfo))) {
            return LERR_FAULT;
        }
        return 0;
    }

    if (request == OHOS_NETDEV_GET_MAC) {
        task_fd_t *slot = task_fd_slot(fd);
        if (!slot || !slot->used || slot->kind != TASK_FD_NET) {
            return LERR_FAULT;
        }
        netdev_t *dev = netdev_first();
        if (!dev) return LERR_IO;
        u8 mac[6];
        for (int i = 0; i < 6; i++) mac[i] = dev->mac[i];
        if (!task_copy_to_user(argp, mac, 6)) {
            return LERR_FAULT;
        }
        return 0;
    }

    return LERR_NOTTY;
}

static int sys_fcntl(registers_t *regs) {
    int fd = (int) regs->ebx;
    int cmd = (int) regs->ecx;
    int arg = (int) regs->edx;
    task_fd_t *slot = task_fd_slot(fd);

    if (!slot || !slot->used) {
        return LERR_BADF;
    }

    switch (cmd) {
        case LINUX_F_GETFD:
            return (slot->flags & LINUX_O_CLOEXEC) ? LINUX_FD_CLOEXEC : 0;
        case LINUX_F_SETFD:
            if ((arg & LINUX_FD_CLOEXEC) != 0) {
                slot->flags |= LINUX_O_CLOEXEC;
            } else {
                slot->flags &= ~LINUX_O_CLOEXEC;
            }
            return 0;
        case LINUX_F_GETFL:
            return slot->flags;
        case LINUX_F_SETFL:
            slot->flags =
                (slot->flags & (LINUX_O_ACCMODE | LINUX_O_CLOEXEC)) |
                (arg & (LINUX_O_APPEND | LINUX_O_NONBLOCK));
            return 0;
        case LINUX_F_DUPFD:
            if (arg < 0 || arg >= TASK_MAX_FDS) {
                return LERR_BADF;
            }
            return task_dup(fd, arg);
        case LINUX_F_DUPFD_CLOEXEC: {
            int newfd;
            if (arg < 0 || arg >= TASK_MAX_FDS) {
                return LERR_BADF;
            }
            newfd = task_dup(fd, arg);
            if (newfd >= 0) {
                task_fd_t *new_slot = task_fd_slot(newfd);
                if (new_slot) {
                    new_slot->flags |= LINUX_O_CLOEXEC;
                }
            }
            return newfd;
        }
        default:
            return LERR_INVAL;
    }
}

static int sys_gettimeofday(registers_t *regs) {
    struct linux_timeval tv;

    if (regs->ebx == 0) {
        return 0;
    }

    tv.tv_sec = (i32)uptime_seconds();
    tv.tv_usec = (i32)(uptime_nanoseconds() / 1000u);
    if (!task_copy_to_user((void *)(uintptr_t)regs->ebx, &tv, sizeof(tv))) {
        return LERR_FAULT;
    }
    return 0;
}

static int sys_readlink_common(const char *path, void *user_buffer, size_t size) {
    const task_state_t *state = task_state();
    const char *target = NULL;
    size_t length;

    if (strcmp(path, "/proc/self/exe") == 0) {
        target = state->path[0] ? state->path : "/bin/unknown";
    } else if (strcmp(path, "/proc/self/cwd") == 0) {
        target = vfs_path(state->cwd ? state->cwd : vfs_root());
    } else {
        return LERR_NOENT;
    }

    length = strlen(target);
    if (size == 0) {
        return 0;
    }
    if (length > size) {
        length = size;
    }
    if (!task_copy_to_user(user_buffer, target, length)) {
        return LERR_FAULT;
    }
    return (int)length;
}

static int sys_readlink(registers_t *regs) {
    char path[SYSCALL_PATH_MAX];

    if (!task_copy_string_from_user(path, sizeof(path), (const char *)(uintptr_t)regs->ebx)) {
        return LERR_FAULT;
    }
    return sys_readlink_common(path, (void *)(uintptr_t)regs->ecx, regs->edx);
}

static int sys_munmap(registers_t *regs) {
    return task_munmap((void *)(uintptr_t)regs->ebx, regs->ecx);
}

static int sys_brk(registers_t *regs) {
    return (int)task_brk(regs->ebx);
}

static int sys_uname(registers_t *regs) {
    struct linux_utsname name;

    memset(&name, 0, sizeof(name));
    strcpy(name.sysname, "OpenHobbyOS");
    strcpy(name.nodename, "openhobby");
    strcpy(name.release, "0.2.0");
    strcpy(name.version, "linux-abi-expansion");
    strcpy(name.machine, "i386");
    strcpy(name.domainname, "local");

    if (!task_copy_to_user((void *)(uintptr_t)regs->ebx, &name, sizeof(name))) {
        return LERR_FAULT;
    }
    return 0;
}

static int sys_writev(registers_t *regs) {
    struct linux_iovec iov[SYSCALL_IOV_MAX];
    int fd = (int)regs->ebx;
    int iovcnt = (int)regs->edx;
    int total = 0;

    if (iovcnt < 0 || iovcnt > SYSCALL_IOV_MAX) {
        return LERR_INVAL;
    }
    if (!task_copy_from_user(iov, (const void *)(uintptr_t)regs->ecx, sizeof(struct linux_iovec) * (size_t)iovcnt)) {
        return LERR_FAULT;
    }

    for (int i = 0; i < iovcnt; ++i) {
        int written;
        if (!task_validate_user_range((uintptr_t)iov[i].iov_base, iov[i].iov_len)) {
            return total ? total : LERR_FAULT;
        }
        written = (int)task_write_fd(fd, iov[i].iov_base, iov[i].iov_len);
        if (written < 0) {
            return total ? total : written;
        }
        total += written;
        if ((u32)written < iov[i].iov_len) {
            break;
        }
    }

    return total;
}

static int sys_readv(registers_t *regs) {
    struct linux_iovec iov[SYSCALL_IOV_MAX];
    int fd = (int)regs->ebx;
    int iovcnt = (int)regs->edx;
    int total = 0;

    if (iovcnt < 0 || iovcnt > SYSCALL_IOV_MAX) {
        return LERR_INVAL;
    }
    if (!task_copy_from_user(iov, (const void *)(uintptr_t)regs->ecx, sizeof(struct linux_iovec) * (size_t)iovcnt)) {
        return LERR_FAULT;
    }

    for (int i = 0; i < iovcnt; ++i) {
        int read_result;
        if (!task_validate_user_range((uintptr_t)iov[i].iov_base, iov[i].iov_len)) {
            return total ? total : LERR_FAULT;
        }
        read_result = (int)task_read_fd(fd, iov[i].iov_base, iov[i].iov_len);
        if (read_result < 0) {
            return total ? total : read_result;
        }
        total += read_result;
        if ((u32)read_result < iov[i].iov_len) {
            break;
        }
    }

    return total;
}

static int sys_nanosleep(registers_t *regs) {
    struct linux_timespec req;
    u64 sleep_ticks;
    u64 target;

    if (!task_copy_from_user(&req, (const void *)(uintptr_t)regs->ebx, sizeof(req))) {
        return LERR_FAULT;
    }

    if (req.tv_sec < 0 || req.tv_nsec < 0 || req.tv_nsec >= 1000000000) {
        return LERR_INVAL;
    }

    sleep_ticks = (u64)req.tv_sec * pit_frequency() +
                  divide_u64_u32((u64)req.tv_nsec * pit_frequency(), 1000000000u);
    if (sleep_ticks == 0 && (req.tv_sec != 0 || req.tv_nsec != 0)) {
        sleep_ticks = 1;
    }
    target = (u64)pit_ticks() + sleep_ticks;

    if (regs->ecx != 0) {
        struct linux_timespec rem;
        rem.tv_sec = 0;
        rem.tv_nsec = 0;
        if (!task_copy_to_user((void *)(uintptr_t)regs->ecx, &rem, sizeof(rem))) {
            return LERR_FAULT;
        }
    }

    task_sleep_current(regs, (u32)target, 0);
}

static int sys_getcwd(registers_t *regs) {
    return task_getcwd((void *)(uintptr_t)regs->ebx, regs->ecx);
}

static int sys_mmap2(registers_t *regs) {
    return (int)(uintptr_t)task_mmap((void *)(uintptr_t)regs->ebx,
                                     regs->ecx,
                                     (int)regs->edx,
                                     (int)regs->esi,
                                     (int)regs->edi,
                                     regs->ebp);
}

static int sys_stat64(registers_t *regs) {
    char path[SYSCALL_PATH_MAX];

    if (regs->ebx == 0) {
        return LERR_FAULT;
    }
    if (!task_copy_string_from_user(path, sizeof(path), (const char *)(uintptr_t)regs->ebx)) {
        return LERR_FAULT;
    }
    return task_stat_path(path, (void *)(uintptr_t)regs->ecx, sizeof(struct linux_stat64));
}

static int sys_fstat64(registers_t *regs) {
    return task_stat_fd((int)regs->ebx, (void *)(uintptr_t)regs->ecx, sizeof(struct linux_stat64));
}

static int sys_getuid32(UNUSED registers_t *regs) {
    return 0;
}

static int sys_getgid32(UNUSED registers_t *regs) {
    return 0;
}

static int sys_geteuid32(UNUSED registers_t *regs) {
    return 0;
}

static int sys_getegid32(UNUSED registers_t *regs) {
    return 0;
}

static int sys_getdents64(registers_t *regs) {
    return task_getdents64((int)regs->ebx, (void *)(uintptr_t)regs->ecx, regs->edx);
}

static int sys_clock_gettime(registers_t *regs) {
    struct linux_timespec spec;

    if (regs->ebx != LINUX_CLOCK_REALTIME && regs->ebx != LINUX_CLOCK_MONOTONIC) {
        return LERR_INVAL;
    }

    spec.tv_sec = (i32)uptime_seconds();
    spec.tv_nsec = (i32)uptime_nanoseconds();
    if (!task_copy_to_user((void *)(uintptr_t)regs->ecx, &spec, sizeof(spec))) {
        return LERR_FAULT;
    }
    return 0;
}

static int sys_openat(registers_t *regs) {
    char path[SYSCALL_PATH_MAX];

    if (!task_copy_string_from_user(path, sizeof(path), (const char *)(uintptr_t)regs->ecx)) {
        return LERR_FAULT;
    }
    return task_open_relative((int)regs->ebx, path, (int)regs->edx, (int)regs->esi);
}

static int sys_readlinkat(registers_t *regs) {
    char path[SYSCALL_PATH_MAX];

    (void)regs->ebx;

    if (!task_copy_string_from_user(path, sizeof(path), (const char *)(uintptr_t)regs->ecx)) {
        return LERR_FAULT;
    }
    return sys_readlink_common(path, (void *)(uintptr_t)regs->edx, regs->esi);
}

static int sys_chdir(registers_t *regs) {
    char path[SYSCALL_PATH_MAX];

    if (!task_copy_string_from_user(path, sizeof(path), (const char *)(uintptr_t)regs->ebx)) {
        return LERR_FAULT;
    }
    return task_chdir(path);
}

static int sys_oh_spawn(registers_t *regs) {
    (void)regs->edx;
    return task_spawn_from_user((const char *)(uintptr_t)regs->ebx, (const u32 *)(uintptr_t)regs->ecx);
}

static int sys_oh_waitpid(registers_t *regs) {
    return task_waitpid_from_user((int)regs->ebx, (void *)(uintptr_t)regs->ecx, (int)regs->edx, regs);
}

static int sys_socket(registers_t *regs) {
    int domain = (int)regs->ebx;
    int type = (int)regs->ecx;
    int protocol = (int)regs->edx;
    socket_endpoint_t *socket = socket_create(domain, type, protocol);
    if (!socket) {
        return LERR_INVAL;
    }
    return task_socket_fd(socket);
}

static int sys_bind(registers_t *regs) {
    int fd = (int)regs->ebx;
    const struct sockaddr *addr = (const struct sockaddr *)(uintptr_t)regs->ecx;
    socklen_t addrlen = (socklen_t)regs->edx;
    socket_endpoint_t *socket = task_get_socket(fd);
    if (!socket) {
        return LERR_BADF;
    }
    if (!task_validate_user_range((uintptr_t)addr, addrlen)) {
        return LERR_FAULT;
    }
    return socket_bind(socket, addr, addrlen);
}

static int sys_listen(registers_t *regs) {
    int fd = (int)regs->ebx;
    int backlog = (int)regs->ecx;
    socket_endpoint_t *socket = task_get_socket(fd);
    if (!socket) {
        return LERR_BADF;
    }
    return socket_listen(socket, backlog);
}

static int sys_accept(registers_t *regs) {
    int fd = (int)regs->ebx;
    struct sockaddr *addr = (struct sockaddr *)(uintptr_t)regs->ecx;
    socklen_t *addrlen = (socklen_t *)(uintptr_t)regs->edx;
    socket_endpoint_t *listener = task_get_socket(fd);
    task_fd_t *slot = task_fd_slot(fd);
    if (!listener) {
        return LERR_BADF;
    }
    socket_endpoint_t *accepted = NULL;
    int result = socket_accept_pending(listener, &accepted);
    if (result == -EAGAIN && slot && (slot->flags & LINUX_O_NONBLOCK) == 0) {
        int slot = task_active_slot_index();
        if (slot < 0) {
            return result;
        }
        listener->accept_waiter = slot;
        listener->accept_addr = (u32)(uintptr_t)addr;
        listener->accept_addrlen = (u32)(uintptr_t)addrlen;
        task_sleep_current(regs, 0xFFFFFFFFu, 0);
    }
    if (result < 0) {
        return result;
    }
    int new_fd = task_socket_fd(accepted);
    if (new_fd < 0) {
        socket_release(accepted);
        return LERR_BADF;
    }
    if (addr && addrlen) {
        socklen_t len;
        if (!task_copy_from_user(&len, addrlen, sizeof(len))) {
            task_close(new_fd);
            return LERR_FAULT;
        }
        result = socket_getpeername(accepted, addr, &len);
        if (result < 0) {
            task_close(new_fd);
            return result;
        }
        if (!task_copy_to_user(addrlen, &len, sizeof(len))) {
            task_close(new_fd);
            return LERR_FAULT;
        }
    }
    return new_fd;
}

static int sys_connect(registers_t *regs) {
    int fd = (int)regs->ebx;
    const struct sockaddr *addr = (const struct sockaddr *)(uintptr_t)regs->ecx;
    socklen_t addrlen = (socklen_t)regs->edx;
    socket_endpoint_t *socket = task_get_socket(fd);
    if (!socket) {
        return LERR_BADF;
    }
    if (!task_validate_user_range((uintptr_t)addr, addrlen)) {
        return LERR_FAULT;
    }
    int result = socket_connect(socket, addr, addrlen);
    return result;
}

static int sys_recv(registers_t *regs) {
    int fd = (int)regs->ebx;
    void *buffer = (void *)(uintptr_t)regs->ecx;
    size_t length = (size_t)regs->edx;
    int flags = (int)regs->esi;
    socket_endpoint_t *socket = task_get_socket(fd);
    task_fd_t *slot = task_fd_slot(fd);
    if (!socket) {
        return LERR_BADF;
    }
    if (!task_validate_user_range((uintptr_t)buffer, length)) {
        return LERR_FAULT;
    }
    interrupts_enable();
    ssize_t result = socket_recv(socket, buffer, length, flags);
    interrupts_disable();
    if (result == -EAGAIN &&
        slot &&
        (slot->flags & LINUX_O_NONBLOCK) == 0 &&
        (flags & LINUX_MSG_DONTWAIT) == 0) {
        int slot = task_active_slot_index();
        if (slot >= 0) {
            socket->read_waiter = slot;
            socket->read_buffer = (u32)(uintptr_t)buffer;
            socket->read_length = (u32)length;
            socket->read_flags = flags;
            socket->waiting_for_read = true;
            task_sleep_current(regs, 0xFFFFFFFFu, 0);
        }
    }
    return (int)result;
}

static u32 linux_cmsg_align(u32 value) {
    return (value + sizeof(u32) - 1u) & ~(sizeof(u32) - 1u);
}

static int sys_sendmsg(registers_t *regs) {
    int fd = (int)regs->ebx;
    const struct linux_msghdr *user_msg = (const struct linux_msghdr *)(uintptr_t)regs->ecx;
    int flags = (int)regs->edx;
    socket_endpoint_t *socket = task_get_socket(fd);
    struct linux_msghdr msg;
    struct linux_iovec iov[SYSCALL_IOV_MAX];
    const vfs_node_t *passed_nodes[SOCKET_CONTROL_MAX];
    char *payload = NULL;
    char *control = NULL;
    u32 total_len = 0;
    u32 copied = 0;
    u32 passed_count = 0;
    int result = 0;

    if (!socket) {
        return LERR_BADF;
    }
    if (!user_msg || !task_copy_from_user(&msg, user_msg, sizeof(msg))) {
        return LERR_FAULT;
    }
    if (msg.msg_iovlen == 0 || msg.msg_iovlen > SYSCALL_IOV_MAX) {
        return LERR_INVAL;
    }
    if (!task_validate_user_range((uintptr_t)msg.msg_iov, msg.msg_iovlen * sizeof(struct linux_iovec)) ||
        !task_copy_from_user(iov, msg.msg_iov, msg.msg_iovlen * sizeof(struct linux_iovec))) {
        return LERR_FAULT;
    }

    for (u32 i = 0; i < msg.msg_iovlen; ++i) {
        if (iov[i].iov_len == 0) {
            continue;
        }
        if (!task_validate_user_range((uintptr_t)iov[i].iov_base, iov[i].iov_len)) {
            return LERR_FAULT;
        }
        total_len += iov[i].iov_len;
    }

    payload = kmalloc(total_len == 0 ? 1u : total_len);
    if (!payload) {
        return LERR_IO;
    }

    for (u32 i = 0; i < msg.msg_iovlen; ++i) {
        if (iov[i].iov_len == 0) {
            continue;
        }
        if (!task_copy_from_user(payload + copied, iov[i].iov_base, iov[i].iov_len)) {
            kfree(payload);
            return LERR_FAULT;
        }
        copied += iov[i].iov_len;
    }

    if (msg.msg_control && msg.msg_controllen > 0) {
        control = kmalloc(msg.msg_controllen);
        if (!control) {
            kfree(payload);
            return LERR_IO;
        }
        if (!task_validate_user_range((uintptr_t)msg.msg_control, msg.msg_controllen) ||
            !task_copy_from_user(control, msg.msg_control, msg.msg_controllen)) {
            kfree(control);
            kfree(payload);
            return LERR_FAULT;
        }

        for (u32 offset = 0; offset + sizeof(struct linux_cmsghdr) <= msg.msg_controllen; ) {
            struct linux_cmsghdr *cmsg = (struct linux_cmsghdr *)(void *)(control + offset);
            u32 header_len = linux_cmsg_align((u32)sizeof(*cmsg));
            u32 data_len;

            if (cmsg->cmsg_len < sizeof(*cmsg) || offset + cmsg->cmsg_len > msg.msg_controllen) {
                break;
            }

            data_len = cmsg->cmsg_len - header_len;
            if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == LINUX_SCM_RIGHTS) {
                int *fds = (int *)(void *)(control + offset + header_len);
                u32 fd_count = data_len / sizeof(int);
                for (u32 i = 0; i < fd_count; ++i) {
                    task_fd_t *passed = task_fd_slot(fds[i]);
                    if (!passed || !passed->used || passed->kind != TASK_FD_FILE || !passed->node) {
                        kfree(control);
                        kfree(payload);
                        return LERR_BADF;
                    }
                    if (passed_count >= SOCKET_CONTROL_MAX) {
                        kfree(control);
                        kfree(payload);
                        return LERR_NFILE;
                    }
                    passed_nodes[passed_count++] = passed->node;
                }
            }

            offset += linux_cmsg_align(cmsg->cmsg_len);
        }
    }

    result = (int)socket_send(socket, payload, total_len, flags);
    if (result >= 0) {
        for (u32 i = 0; i < passed_count; ++i) {
            if (!socket->peer || socket_enqueue_passed_node(socket->peer, passed_nodes[i]) < 0) {
                result = -EAGAIN;
                break;
            }
        }
    }

    if (control) {
        kfree(control);
    }
    kfree(payload);
    return result;
}

static int sys_recvmsg(registers_t *regs) {
    int fd = (int)regs->ebx;
    struct linux_msghdr *user_msg = (struct linux_msghdr *)(uintptr_t)regs->ecx;
    int flags = (int)regs->edx;
    socket_endpoint_t *socket = task_get_socket(fd);
    struct linux_msghdr msg;
    struct linux_iovec iov[SYSCALL_IOV_MAX];
    char *payload = NULL;
    char *control = NULL;
    u32 total_len = 0;
    u32 copied = 0;
    int received;

    if (!socket) {
        return LERR_BADF;
    }
    if (!user_msg || !task_copy_from_user(&msg, user_msg, sizeof(msg))) {
        return LERR_FAULT;
    }
    if (msg.msg_iovlen == 0 || msg.msg_iovlen > SYSCALL_IOV_MAX) {
        return LERR_INVAL;
    }
    if (!task_validate_user_range((uintptr_t)msg.msg_iov, msg.msg_iovlen * sizeof(struct linux_iovec)) ||
        !task_copy_from_user(iov, msg.msg_iov, msg.msg_iovlen * sizeof(struct linux_iovec))) {
        return LERR_FAULT;
    }

    for (u32 i = 0; i < msg.msg_iovlen; ++i) {
        if (iov[i].iov_len == 0) {
            continue;
        }
        if (!task_validate_user_range((uintptr_t)iov[i].iov_base, iov[i].iov_len)) {
            return LERR_FAULT;
        }
        total_len += iov[i].iov_len;
    }

    payload = kmalloc(total_len == 0 ? 1u : total_len);
    if (!payload) {
        return LERR_IO;
    }

    received = (int)socket_recv(socket, payload, total_len, flags);
    if (received <= 0) {
        kfree(payload);
        return received;
    }

    for (u32 i = 0; i < msg.msg_iovlen && copied < (u32)received; ++i) {
        u32 chunk = iov[i].iov_len;
        if (chunk > (u32)received - copied) {
            chunk = (u32)received - copied;
        }
        if (chunk > 0 && !task_copy_to_user(iov[i].iov_base, payload + copied, chunk)) {
            kfree(payload);
            return LERR_FAULT;
        }
        copied += chunk;
    }

    if (msg.msg_control && msg.msg_controllen >= linux_cmsg_align((u32)sizeof(struct linux_cmsghdr))) {
        u32 header_len = linux_cmsg_align((u32)sizeof(struct linux_cmsghdr));
        u32 max_fds = (msg.msg_controllen - header_len) / sizeof(int);
        u32 installed = 0;

        control = kmalloc(msg.msg_controllen);
        if (!control) {
            kfree(payload);
            return LERR_IO;
        }
        memset(control, 0, msg.msg_controllen);

        while (installed < max_fds) {
            const vfs_node_t *node = NULL;
            int new_fd;
            if (socket_dequeue_passed_node(socket, &node) < 0) {
                break;
            }
            new_fd = task_install_file_fd(node, LINUX_O_RDWR, 0);
            if (new_fd < 0) {
                break;
            }
            ((int *)(void *)(control + header_len))[installed++] = new_fd;
        }

        if (installed > 0) {
            struct linux_cmsghdr *cmsg = (struct linux_cmsghdr *)(void *)control;
            cmsg->cmsg_len = header_len + installed * sizeof(int);
            cmsg->cmsg_level = SOL_SOCKET;
            cmsg->cmsg_type = LINUX_SCM_RIGHTS;
            if (!task_copy_to_user(msg.msg_control, control, cmsg->cmsg_len)) {
                kfree(control);
                kfree(payload);
                return LERR_FAULT;
            }
            msg.msg_controllen = cmsg->cmsg_len;
        } else {
            msg.msg_controllen = 0;
        }
        msg.msg_flags = 0;
    } else {
        msg.msg_controllen = 0;
        msg.msg_flags = 0;
    }

    if (!task_copy_to_user(user_msg, &msg, sizeof(msg))) {
        if (control) {
            kfree(control);
        }
        kfree(payload);
        return LERR_FAULT;
    }

    if (control) {
        kfree(control);
    }
    kfree(payload);
    return received;
}

static int sys_send(registers_t *regs) {
    int fd = (int)regs->ebx;
    const void *buffer = (const void *)(uintptr_t)regs->ecx;
    size_t length = (size_t)regs->edx;
    int flags = (int)regs->esi;
    socket_endpoint_t *socket = task_get_socket(fd);
    if (!socket) {
        return LERR_BADF;
    }
    if (!task_validate_user_range((uintptr_t)buffer, length)) {
        return LERR_FAULT;
    }
    return (int)socket_send(socket, buffer, length, flags);
}

static int sys_shutdown(registers_t *regs) {
    int fd = (int)regs->ebx;
    int how = (int)regs->ecx;
    socket_endpoint_t *socket = task_get_socket(fd);
    if (!socket) {
        return LERR_BADF;
    }
    return socket_shutdown(socket, how);
}

static int sys_getsockname(registers_t *regs) {
    int fd = (int)regs->ebx;
    struct sockaddr *addr = (struct sockaddr *)(uintptr_t)regs->ecx;
    socklen_t *addrlen = (socklen_t *)(uintptr_t)regs->edx;
    socket_endpoint_t *socket = task_get_socket(fd);
    if (!socket) {
        return LERR_BADF;
    }
    socklen_t len;
    if (!task_copy_from_user(&len, addrlen, sizeof(len))) {
        return LERR_FAULT;
    }
    if (!task_validate_user_range((uintptr_t)addr, len)) {
        return LERR_FAULT;
    }
    int result = socket_getsockname(socket, addr, &len);
    if (result < 0) {
        return result;
    }
    if (!task_copy_to_user(addrlen, &len, sizeof(len))) {
        return LERR_FAULT;
    }
    return 0;
}

static int sys_getpeername(registers_t *regs) {
    int fd = (int)regs->ebx;
    struct sockaddr *addr = (struct sockaddr *)(uintptr_t)regs->ecx;
    socklen_t *addrlen = (socklen_t *)(uintptr_t)regs->edx;
    socket_endpoint_t *socket = task_get_socket(fd);
    if (!socket) {
        return LERR_BADF;
    }
    socklen_t len;
    if (!task_copy_from_user(&len, addrlen, sizeof(len))) {
        return LERR_FAULT;
    }
    if (!task_validate_user_range((uintptr_t)addr, len)) {
        return LERR_FAULT;
    }
    int result = socket_getpeername(socket, addr, &len);
    if (result < 0) {
        return result;
    }
    if (!task_copy_to_user(addrlen, &len, sizeof(len))) {
        return LERR_FAULT;
    }
    return 0;
}

static int sys_setsockopt(registers_t *regs) {
    int fd = (int)regs->ebx;
    int level = (int)regs->ecx;
    int optname = (int)regs->edx;
    const void *optval = (const void *)(uintptr_t)regs->esi;
    socklen_t optlen = (socklen_t)regs->edi;
    socket_endpoint_t *socket = task_get_socket(fd);
    if (!socket) {
        return LERR_BADF;
    }
    if (!task_validate_user_range((uintptr_t)optval, optlen)) {
        return LERR_FAULT;
    }
    return socket_setsockopt(socket, level, optname, optval, optlen);
}

static int sys_getsockopt(registers_t *regs) {
    int fd = (int)regs->ebx;
    int level = (int)regs->ecx;
    int optname = (int)regs->edx;
    void *optval = (void *)(uintptr_t)regs->esi;
    socklen_t *optlen = (socklen_t *)(uintptr_t)regs->edi;
    socket_endpoint_t *socket = task_get_socket(fd);
    if (!socket) {
        return LERR_BADF;
    }
    socklen_t len;
    if (!task_copy_from_user(&len, optlen, sizeof(len))) {
        return LERR_FAULT;
    }
    if (!task_validate_user_range((uintptr_t)optval, len)) {
        return LERR_FAULT;
    }
    int result = socket_getsockopt(socket, level, optname, optval, &len);
    if (result < 0) {
        return result;
    }
    if (!task_copy_to_user(optlen, &len, sizeof(len))) {
        return LERR_FAULT;
    }
    return 0;
}

static int sys_oh_yield(registers_t *regs) {
    task_yield_current(regs, 0);
}

static int sys_oh_unlink(registers_t *regs) {
    char path[SYSCALL_PATH_MAX];

    if (!task_copy_string_from_user(path, sizeof(path), (const char *)(uintptr_t)regs->ebx)) {
        return LERR_FAULT;
    }

    /* Support unlink in /tmp for ramfiles */
    if (path_is_in_tmp(path)) {
        return vfs_unlink_ramfile(path) == 0 ? 0 : LERR_NOENT;
    }

    if (socket_unlink_path(path)) {
        return 0;
    }

    return LERR_PERM;
}

static int sys_oh_mkdir(registers_t *regs) {
    char path[SYSCALL_PATH_MAX];

    (void)regs->ecx;

    if (!task_copy_string_from_user(path, sizeof(path), (const char *)(uintptr_t)regs->ebx)) {
        return LERR_FAULT;
    }

    return vfs_mkdir(path);
}

static int sys_link(registers_t *regs) {
    char oldpath[SYSCALL_PATH_MAX];
    char newpath[SYSCALL_PATH_MAX];

    if (!task_copy_string_from_user(oldpath, sizeof(oldpath), (const char *)(uintptr_t)regs->ebx)) {
        return LERR_FAULT;
    }
    if (!task_copy_string_from_user(newpath, sizeof(newpath), (const char *)(uintptr_t)regs->ecx)) {
        return LERR_FAULT;
    }

    /* Only support link within /tmp for now */
    if (!path_is_in_tmp(oldpath) || !path_is_in_tmp(newpath)) {
        return LERR_ROFS;
    }

    /* Check if source exists and is a ramfile */
    const vfs_node_t *oldnode = vfs_resolve(NULL, oldpath);
    if (!oldnode || !vfs_node_is_ramfile(oldnode)) {
        return LERR_NOENT;
    }

    /* Check if destination already exists */
    const vfs_node_t *newnode = vfs_resolve(NULL, newpath);
    if (newnode) {
        return LERR_EXIST;
    }

    /* Perform the link */
    if (vfs_link_ramfile(oldpath, newpath) != 0) {
        return LERR_IO;
    }

    return 0;
}

static int sys_rename(registers_t *regs) {
    char oldpath[SYSCALL_PATH_MAX];
    char newpath[SYSCALL_PATH_MAX];

    if (!task_copy_string_from_user(oldpath, sizeof(oldpath), (const char *)(uintptr_t)regs->ebx)) {
        return LERR_FAULT;
    }
    if (!task_copy_string_from_user(newpath, sizeof(newpath), (const char *)(uintptr_t)regs->ecx)) {
        return LERR_FAULT;
    }

    if (!path_is_in_tmp(oldpath) || !path_is_in_tmp(newpath)) {
        return LERR_ROFS;
    }

    if (vfs_rename_ramfile(oldpath, newpath) == 0) {
        return 0;
    }

    return LERR_NOENT;
}

static int sys_dup(registers_t *regs) {
    int oldfd = (int)regs->ebx;
    task_fd_t *old_slot;
    int newfd;

    old_slot = task_fd_slot(oldfd);
    if (!old_slot || !old_slot->used) {
        return LERR_BADF;
    }

    newfd = task_alloc_fd();
    if (newfd < 0) {
        return newfd;
    }

    current_task.fds[newfd] = *old_slot;
    return newfd;
}

static int sys_dup2(registers_t *regs) {
    int oldfd = (int)regs->ebx;
    int newfd = (int)regs->ecx;
    task_fd_t *old_slot;
    task_fd_t *new_slot;

    if (oldfd == newfd) {
        return oldfd;
    }

    old_slot = task_fd_slot(oldfd);
    if (!old_slot || !old_slot->used) {
        return LERR_BADF;
    }

    new_slot = task_fd_slot(newfd);
    if (new_slot && new_slot->used) {
        task_close(newfd);
    }

    current_task.fds[newfd] = *old_slot;
    return newfd;
}

static int sys_poll(registers_t *regs) {
    struct linux_pollfd *user_fds = (struct linux_pollfd *)(uintptr_t)regs->ebx;
    u32 count = regs->ecx;
    int timeout = (int)regs->edx;
    struct linux_pollfd local_fds[TASK_MAX_FDS];
    int ready = 0;

    if (count > TASK_MAX_FDS) {
        count = TASK_MAX_FDS;
    }

    if (!task_copy_from_user(local_fds, user_fds, count * sizeof(struct linux_pollfd))) {
        return LERR_FAULT;
    }

    for (u32 i = 0; i < count; ++i) {
        local_fds[i].revents = 0;

        if (local_fds[i].fd < 0) {
            local_fds[i].revents = LINUX_POLLNVAL;
            ready++;
            continue;
        }

        task_fd_t *slot = task_fd_slot((int)local_fds[i].fd);
        if (!slot || !slot->used) {
            local_fds[i].revents = LINUX_POLLNVAL;
            ready++;
            continue;
        }

        u16 events = local_fds[i].events;

        if ((events & (LINUX_POLLIN | LINUX_POLLPRI)) != 0) {
            bool can_read = false;

            if (slot->kind == TASK_FD_CONSOLE) {
                can_read = console_term_resp_available() || keyboard_has_input();
            } else if (slot->kind == TASK_FD_PIPE_READ && slot->pipe) {
                can_read = pipe_pending_readable((pipe_endpoint_t *)slot->pipe) > 0;
            } else if (slot->kind == TASK_FD_SOCKET && slot->socket) {
                can_read = socket_pending_readable((socket_endpoint_t *)slot->socket) > 0;
            } else if (slot->kind == TASK_FD_FILE || slot->kind == TASK_FD_NULL || slot->kind == TASK_FD_FB0) {
                can_read = true;
            }

            if (can_read) {
                local_fds[i].revents |= LINUX_POLLIN;
                ready++;
            }
        }

        if ((events & LINUX_POLLOUT) != 0) {
            bool can_write = true;
            if (slot->kind == TASK_FD_PIPE_WRITE && slot->pipe) {
                pipe_endpoint_t *p = (pipe_endpoint_t *)slot->pipe;
                can_write = p->write_open && !p->peer_closed;
            }
            if (can_write) {
                local_fds[i].revents |= LINUX_POLLOUT;
                ready++;
            }
        }
    }

    if (ready == 0 && timeout != 0) {
        if (!task_copy_to_user(user_fds, local_fds, count * sizeof(struct linux_pollfd))) {
            return LERR_FAULT;
        }
        if (timeout > 0) {
            u32 hz = pit_frequency();
            u32 sleep_ticks = (u32)((u32)timeout * hz / 1000u);
            if (sleep_ticks == 0) sleep_ticks = 1;
            task_sleep_current(regs, pit_ticks() + sleep_ticks, 0);
        } else {
            /* timeout < 0: wait indefinitely (sleep until woken by data) */
            task_sleep_current(regs, 0xFFFFFFFFu, 0);
        }
        return 0;
    }

    if (!task_copy_to_user(user_fds, local_fds, count * sizeof(struct linux_pollfd))) {
        return LERR_FAULT;
    }

    return ready;
}

static int sys_pipe(registers_t *regs) {
    int *pipefd = (int *)(uintptr_t)regs->ebx;
    int readfd, writefd;
    pipe_endpoint_t *pipe;

    if (!pipefd) {
        return LERR_FAULT;
    }

    pipe = pipe_create();
    if (!pipe) {
        return LERR_NFILE;
    }

    readfd = task_alloc_fd();
    if (readfd < 0) {
        pipe_release(pipe);
        return LERR_NFILE;
    }

    writefd = task_alloc_fd();
    if (writefd < 0) {
        current_task.fds[readfd].used = false;
        pipe_release(pipe);
        return LERR_NFILE;
    }

    /* Increment refcount for second FD (pipe_create starts with 1) */
    pipe->refcount++;

    current_task.fds[readfd].used = true;
    current_task.fds[readfd].kind = TASK_FD_PIPE_READ;
    current_task.fds[readfd].flags = LINUX_O_RDONLY;
    current_task.fds[readfd].offset = 0;
    current_task.fds[readfd].node = NULL;
    current_task.fds[readfd].pipe = pipe;

    current_task.fds[writefd].used = true;
    current_task.fds[writefd].kind = TASK_FD_PIPE_WRITE;
    current_task.fds[writefd].flags = LINUX_O_WRONLY;
    current_task.fds[writefd].offset = 0;
    current_task.fds[writefd].node = NULL;
    current_task.fds[writefd].pipe = pipe;

    if (!task_copy_to_user(&pipefd[0], &readfd, sizeof(int)) ||
        !task_copy_to_user(&pipefd[1], &writefd, sizeof(int))) {
        current_task.fds[readfd].used = false;
        current_task.fds[writefd].used = false;
        pipe_release(pipe);
        pipe_release(pipe);
        return LERR_FAULT;
    }

    return 0;
}

static int sys_oh_reboot(UNUSED registers_t *regs) {
    console_write("reboot syscall invoked\n");
    power_reboot();
    return 0;
}

static int sys_oh_shutdown(UNUSED registers_t *regs) {
    return power_shutdown() ? 1 : 0;
}

static int sys_oh_suspend(UNUSED registers_t *regs) {
    return power_suspend() ? 1 : 0;
}

static int sys_oh_memstat(registers_t *regs) {
    u32 *user_buf = (u32 *)(uintptr_t)regs->ebx;
    memory_stats_t stats = memory_stats();
    u32 buf[5];

    buf[0] = stats.total_bytes;
    buf[1] = stats.heap_start;
    buf[2] = stats.heap_end;
    buf[3] = stats.heap_used;
    buf[4] = stats.heap_free;

    if (!task_validate_user_range((uintptr_t)user_buf, sizeof(buf)) ||
        !task_copy_to_user(user_buf, buf, sizeof(buf))) {
        return -1;
    }
    return 0;
}

static int sys_oh_ticks(UNUSED registers_t *regs) {
    return (int)pit_ticks();
}

static int sys_oh_tickfreq(UNUSED registers_t *regs) {
    return (int)pit_frequency();
}

int syscall_dispatch(registers_t *regs) {
    u32 number = regs->eax;
    int result;

    switch (number) {
        case LINUX_SYS_EXIT:
            result = sys_exit(regs);
            break;
        case LINUX_SYS_READ:
            result = sys_read(regs);
            break;
        case LINUX_SYS_WRITE:
            result = sys_write(regs);
            break;
        case LINUX_SYS_OPEN:
            result = sys_open(regs);
            break;
        case LINUX_SYS_CLOSE:
            result = sys_close(regs);
            break;
        case LINUX_SYS_CHDIR:
            result = sys_chdir(regs);
            break;
        case LINUX_SYS_LINK:
            result = sys_link(regs);
            break;
        case LINUX_SYS_RENAME:
            result = sys_rename(regs);
            break;
        case LINUX_SYS_LSEEK:
            result = sys_lseek(regs);
            break;
        case LINUX_SYS_GETPID:
            result = sys_getpid(regs);
            break;
        case LINUX_SYS_FORK:
            result = sys_fork(regs);
            break;
        case LINUX_SYS_EXECVE:
            result = sys_execve(regs);
            break;
        case LINUX_SYS_ACCESS:
            result = sys_access(regs);
            break;
        case LINUX_SYS_FCNTL:
            result = sys_fcntl(regs);
            break;
        case LINUX_SYS_IOCTL:
            result = sys_ioctl(regs);
            break;
        case LINUX_SYS_GETTIMEOFDAY:
            result = sys_gettimeofday(regs);
            break;
        case LINUX_SYS_READLINK:
            result = sys_readlink(regs);
            break;
        case LINUX_SYS_MUNMAP:
            result = sys_munmap(regs);
            break;
        case LINUX_SYS_BRK:
            result = sys_brk(regs);
            break;
        case LINUX_SYS_UNAME:
            result = sys_uname(regs);
            break;
        case LINUX_SYS_READV:
            result = sys_readv(regs);
            break;
        case LINUX_SYS_WRITEV:
            result = sys_writev(regs);
            break;
        case LINUX_SYS_NANOSLEEP:
            result = sys_nanosleep(regs);
            break;
        case LINUX_SYS_GETCWD:
            result = sys_getcwd(regs);
            break;
        case LINUX_SYS_MMAP2:
            result = sys_mmap2(regs);
            break;
        case LINUX_SYS_STAT64:
            result = sys_stat64(regs);
            break;
        case LINUX_SYS_FSTAT64:
            result = sys_fstat64(regs);
            break;
        case LINUX_SYS_GETUID32:
            result = sys_getuid32(regs);
            break;
        case LINUX_SYS_GETGID32:
            result = sys_getgid32(regs);
            break;
        case LINUX_SYS_GETEUID32:
            result = sys_geteuid32(regs);
            break;
        case LINUX_SYS_GETEGID32:
            result = sys_getegid32(regs);
            break;
        case LINUX_SYS_GETDENTS64:
            result = sys_getdents64(regs);
            break;
        case LINUX_SYS_EXIT_GROUP:
            result = sys_exit_group(regs);
            break;
        case LINUX_SYS_CLOCK_GETTIME:
            result = sys_clock_gettime(regs);
            break;
        case LINUX_SYS_OPENAT:
            result = sys_openat(regs);
            break;
        case LINUX_SYS_READLINKAT:
            result = sys_readlinkat(regs);
            break;
        case LINUX_SYS_SOCKET:
            result = sys_socket(regs);
            break;
        case LINUX_SYS_BIND:
            result = sys_bind(regs);
            break;
        case LINUX_SYS_CONNECT:
            result = sys_connect(regs);
            break;
        case LINUX_SYS_LISTEN:
            result = sys_listen(regs);
            break;
        case LINUX_SYS_ACCEPT:
            result = sys_accept(regs);
            break;
        case LINUX_SYS_GETSOCKNAME:
            result = sys_getsockname(regs);
            break;
        case LINUX_SYS_GETPEERNAME:
            result = sys_getpeername(regs);
            break;
        case LINUX_SYS_DUP:
            result = sys_dup(regs);
            break;
        case LINUX_SYS_DUP2:
            result = sys_dup2(regs);
            break;
        case LINUX_SYS_POLL:
            result = sys_poll(regs);
            break;
        case LINUX_SYS_PIPE:
            result = sys_pipe(regs);
            break;
        case LINUX_SYS_SEND:
            result = sys_send(regs);
            break;
        case LINUX_SYS_RECV:
            result = sys_recv(regs);
            break;
        case OHOS_SYS_SENDMSG:
            result = sys_sendmsg(regs);
            break;
        case OHOS_SYS_RECVMSG:
            result = sys_recvmsg(regs);
            break;
        case LINUX_SYS_SHUTDOWN:
            result = sys_shutdown(regs);
            break;
        case LINUX_SYS_SETSOCKOPT:
            result = sys_setsockopt(regs);
            break;
        case LINUX_SYS_GETSOCKOPT:
            result = sys_getsockopt(regs);
            break;
        case OHOS_SYS_SPAWN:
            result = sys_oh_spawn(regs);
            break;
        case OHOS_SYS_WAITPID:
            result = sys_oh_waitpid(regs);
            break;
        case OHOS_SYS_YIELD:
            result = sys_oh_yield(regs);
            break;
        case OHOS_SYS_UNLINK:
            result = sys_oh_unlink(regs);
            break;
        case OHOS_SYS_MKDIR:
            result = sys_oh_mkdir(regs);
            break;
        case OHOS_SYS_REBOOT:
            result = sys_oh_reboot(regs);
            break;
        case OHOS_SYS_SHUTDOWN:
            result = sys_oh_shutdown(regs);
            break;
        case OHOS_SYS_SUSPEND:
            result = sys_oh_suspend(regs);
            break;
        case OHOS_SYS_MEMSTAT:
            result = sys_oh_memstat(regs);
            break;
        case OHOS_SYS_TICKS:
            result = sys_oh_ticks(regs);
            break;
        case OHOS_SYS_TICKFREQ:
            result = sys_oh_tickfreq(regs);
            break;
        default:
            result = LERR_INVAL;
            break;
    }

    return result;
}
