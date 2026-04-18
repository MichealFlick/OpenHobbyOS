#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdarg.h>
#include <reent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#ifdef st_atime
#undef st_atime
#endif
#ifdef st_mtime
#undef st_mtime
#endif
#ifdef st_ctime
#undef st_ctime
#endif

#include "openflags.h"
#include "abi/linux.h"

static inline int oh_syscall0(int number) {
    int result;
    __asm__ volatile ("int $0x80" : "=a"(result) : "a"(number) : "memory");
    return result;
}

static inline int oh_syscall1(int number, int arg1) {
    int result;
    __asm__ volatile ("int $0x80" : "=a"(result) : "a"(number), "b"(arg1) : "memory");
    return result;
}

static inline int oh_syscall2(int number, int arg1, int arg2) {
    int result;
    __asm__ volatile ("int $0x80"
                      : "=a"(result)
                      : "a"(number), "b"(arg1), "c"(arg2)
                      : "memory");
    return result;
}

static inline int oh_syscall3(int number, int arg1, int arg2, int arg3) {
    int result;
    __asm__ volatile ("int $0x80"
                      : "=a"(result)
                      : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3)
                      : "memory");
    return result;
}

static void oh_set_reent_errno(struct _reent *reent, int result) {
    if (result < 0) {
        reent->_errno = -result;
    }
}

static int oh_set_errno_result(int result) {
    if (result < 0) {
        errno = -result;
        return -1;
    }

    return result;
}

static ssize_t oh_set_errno_ssize(int result) {
    if (result < 0) {
        errno = -result;
        return -1;
    }

    return (ssize_t) result;
}

static void oh_copy_stat(struct stat *out, const struct linux_stat64 *in) {
    out->st_dev = (dev_t)in->st_dev;
    out->st_ino = (ino_t)in->__st_ino;
    out->st_mode = (mode_t)in->st_mode;
    out->st_nlink = (nlink_t)in->st_nlink;
    out->st_uid = (uid_t)in->st_uid;
    out->st_gid = (gid_t)in->st_gid;
    out->st_rdev = (dev_t)in->st_rdev;
    out->st_size = (off_t)in->st_size;
    out->st_atim.tv_sec = (time_t)in->st_atime;
    out->st_atim.tv_nsec = 0;
    out->st_mtim.tv_sec = (time_t)in->st_mtime;
    out->st_mtim.tv_nsec = 0;
    out->st_ctim.tv_sec = (time_t)in->st_ctime;
    out->st_ctim.tv_nsec = 0;
    out->st_blksize = (blksize_t)in->st_blksize;
    out->st_blocks = (blkcnt_t)in->st_blocks;
}

void _exit(int status) {
    oh_syscall1(LINUX_SYS_EXIT_GROUP, status);
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void _exit_r(struct _reent *reent, int status) {
    (void) reent;
    _exit(status);
}

int close(int fd) {
    int result = oh_syscall1(LINUX_SYS_CLOSE, fd);
    return oh_set_errno_result(result);
}

int open(const char *path, int flags, ...) {
    mode_t mode = 0;

    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = (mode_t) va_arg(args, int);
        va_end(args);
    }

    return oh_set_errno_result(oh_syscall3(LINUX_SYS_OPEN, (int) path, oh_translate_open_flags(flags), mode));
}

ssize_t read(int fd, void *buffer, size_t length) {
    return oh_set_errno_ssize(oh_syscall3(LINUX_SYS_READ, fd, (int) buffer, (int) length));
}

ssize_t write(int fd, const void *buffer, size_t length) {
    return oh_set_errno_ssize(oh_syscall3(LINUX_SYS_WRITE, fd, (int) buffer, (int) length));
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
    return oh_set_errno_ssize(oh_syscall3(LINUX_SYS_READV, fd, (int) iov, iovcnt));
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
    return oh_set_errno_ssize(oh_syscall3(LINUX_SYS_WRITEV, fd, (int) iov, iovcnt));
}

off_t lseek(int fd, off_t offset, int whence) {
    int result = oh_syscall3(LINUX_SYS_LSEEK, fd, (int)offset, whence);
    if (result < 0) {
        errno = -result;
        return (off_t) -1;
    }

    return (off_t) result;
}

int fstat(int fd, struct stat *statbuf) {
    struct linux_stat64 native_stat;
    int result = oh_syscall2(LINUX_SYS_FSTAT64, fd, (int) &native_stat);

    if (result < 0) {
        errno = -result;
        return -1;
    }

    oh_copy_stat(statbuf, &native_stat);
    return 0;
}

int stat(const char *path, struct stat *statbuf) {
    struct linux_stat64 native_stat;
    int result = oh_syscall2(LINUX_SYS_STAT64, (int) path, (int) &native_stat);

    if (result < 0) {
        errno = -result;
        return -1;
    }

    oh_copy_stat(statbuf, &native_stat);
    return 0;
}

int lstat(const char *path, struct stat *statbuf) {
    return stat(path, statbuf);
}

int isatty(int fd) {
    struct linux_termios termios;
    int result = oh_syscall3(LINUX_SYS_IOCTL, fd, LINUX_TCGETS, (int) &termios);

    if (result < 0) {
        errno = ENOTTY;
        return 0;
    }

    return 1;
}

void *sbrk(ptrdiff_t increment) {
    static char *heap_end;
    char *previous;
    char *next;
    int result;

    if (heap_end == 0) {
        heap_end = (char *) oh_syscall1(LINUX_SYS_BRK, 0);
    }

    previous = heap_end;
    next = heap_end + increment;
    result = oh_syscall1(LINUX_SYS_BRK, (int) next);
    if (result != (int) next) {
        errno = ENOMEM;
        return (void *) -1;
    }

    heap_end = next;
    return previous;
}

int access(const char *path, int mode) {
    return oh_set_errno_result(oh_syscall2(LINUX_SYS_ACCESS, (int) path, mode));
}

char *getcwd(char *buffer, size_t size) {
    int result = oh_syscall2(LINUX_SYS_GETCWD, (int) buffer, (int) size);
    if (result < 0) {
        errno = -result;
        return NULL;
    }

    return buffer;
}

ssize_t readlink(const char *path, char *buffer, size_t size) {
    return oh_set_errno_ssize(oh_syscall3(LINUX_SYS_READLINK, (int) path, (int) buffer, (int) size));
}

int unlink(const char *path) {
    (void) path;
    errno = EROFS;
    return -1;
}

int mkdir(const char *path, mode_t mode) {
    (void) path;
    (void) mode;
    errno = EROFS;
    return -1;
}

int link(const char *existing, const char *new_link) {
    (void) existing;
    (void) new_link;
    errno = EROFS;
    return -1;
}

int rename(const char *old_name, const char *new_name) {
    (void) old_name;
    (void) new_name;
    errno = EROFS;
    return -1;
}

pid_t wait(int *status) {
    (void) status;
    errno = ENOSYS;
    return -1;
}

clock_t times(struct tms *buffer) {
    (void) buffer;
    errno = ENOSYS;
    return (clock_t) -1;
}

int getentropy(void *buffer, size_t length) {
    unsigned char *bytes = (unsigned char *) buffer;
    int seed = oh_syscall0(LINUX_SYS_GETPID);

    for (size_t i = 0; i < length; ++i) {
        seed = seed * 1103515245 + 12345;
        bytes[i] = (unsigned char) (seed >> 16);
    }

    return 0;
}
