#include "syscall.h"

#include "abi/linux.h"
#include "console.h"
#include "io.h"
#include "pit.h"
#include "string.h"
#include "task.h"

#define SYSCALL_PATH_MAX 256
#define SYSCALL_IOV_MAX  16

#define LERR_PERM    (-1)
#define LERR_NOENT   (-2)
#define LERR_INTR    (-4)
#define LERR_BADF    (-9)
#define LERR_FAULT   (-14)
#define LERR_NOTTY   (-25)
#define LERR_INVAL   (-22)

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
        winsize.ws_row = 25;
        winsize.ws_col = 80;
        winsize.ws_xpixel = 0;
        winsize.ws_ypixel = 0;
        if (!task_is_console_fd(fd) || !task_copy_to_user(argp, &winsize, sizeof(winsize))) {
            return LERR_FAULT;
        }
        return 0;
    }

    return LERR_NOTTY;
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
    struct linux_timespec rem;
    u64 start;
    u64 target;

    if (!task_copy_from_user(&req, (const void *)(uintptr_t)regs->ebx, sizeof(req))) {
        return LERR_FAULT;
    }

    if (req.tv_sec < 0 || req.tv_nsec < 0 || req.tv_nsec >= 1000000000) {
        return LERR_INVAL;
    }

    start = pit_ticks();
    target = start +
             (u64)req.tv_sec * pit_frequency() +
             divide_u64_u32((u64)req.tv_nsec * pit_frequency(), 1000000000u);

    interrupts_enable();
    while ((u64)pit_ticks() < target) {
        cpu_halt();
    }
    interrupts_disable();

    if (regs->ecx != 0) {
        rem.tv_sec = 0;
        rem.tv_nsec = 0;
        if (!task_copy_to_user((void *)(uintptr_t)regs->ecx, &rem, sizeof(rem))) {
            return LERR_FAULT;
        }
    }
    return 0;
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

int syscall_dispatch(registers_t *regs) {
    switch (regs->eax) {
        case LINUX_SYS_EXIT:
            return sys_exit(regs);
        case LINUX_SYS_READ:
            return sys_read(regs);
        case LINUX_SYS_WRITE:
            return sys_write(regs);
        case LINUX_SYS_OPEN:
            return sys_open(regs);
        case LINUX_SYS_CLOSE:
            return sys_close(regs);
        case LINUX_SYS_CHDIR:
            return sys_chdir(regs);
        case LINUX_SYS_LSEEK:
            return sys_lseek(regs);
        case LINUX_SYS_GETPID:
            return sys_getpid(regs);
        case LINUX_SYS_ACCESS:
            return sys_access(regs);
        case LINUX_SYS_IOCTL:
            return sys_ioctl(regs);
        case LINUX_SYS_GETTIMEOFDAY:
            return sys_gettimeofday(regs);
        case LINUX_SYS_READLINK:
            return sys_readlink(regs);
        case LINUX_SYS_MUNMAP:
            return sys_munmap(regs);
        case LINUX_SYS_BRK:
            return sys_brk(regs);
        case LINUX_SYS_UNAME:
            return sys_uname(regs);
        case LINUX_SYS_READV:
            return sys_readv(regs);
        case LINUX_SYS_WRITEV:
            return sys_writev(regs);
        case LINUX_SYS_NANOSLEEP:
            return sys_nanosleep(regs);
        case LINUX_SYS_GETCWD:
            return sys_getcwd(regs);
        case LINUX_SYS_MMAP2:
            return sys_mmap2(regs);
        case LINUX_SYS_STAT64:
            return sys_stat64(regs);
        case LINUX_SYS_FSTAT64:
            return sys_fstat64(regs);
        case LINUX_SYS_GETUID32:
            return sys_getuid32(regs);
        case LINUX_SYS_GETGID32:
            return sys_getgid32(regs);
        case LINUX_SYS_GETEUID32:
            return sys_geteuid32(regs);
        case LINUX_SYS_GETEGID32:
            return sys_getegid32(regs);
        case LINUX_SYS_GETDENTS64:
            return sys_getdents64(regs);
        case LINUX_SYS_EXIT_GROUP:
            return sys_exit_group(regs);
        case LINUX_SYS_CLOCK_GETTIME:
            return sys_clock_gettime(regs);
        case LINUX_SYS_OPENAT:
            return sys_openat(regs);
        case LINUX_SYS_READLINKAT:
            return sys_readlinkat(regs);
        default:
            return LERR_INVAL;
    }
}
