/* Newlib glue - connects newlib syscalls to OpenHobbyOS kernel syscalls */

#include <stddef.h>
#include "abi/linux.h"
#include "syscall.h"

/* Minimal type definitions for newlib compatibility */
typedef long off_t;
typedef long clock_t;

struct tms {
    clock_t tms_utime;
    clock_t tms_stime;
    clock_t tms_cutime;
    clock_t tms_cstime;
};

struct stat {
    unsigned long st_dev;
    unsigned int st_mode;
    unsigned long st_size;
    unsigned long st_blksize;
    unsigned long st_blocks;
};

struct timeval {
    long tv_sec;
    long tv_usec;
};

/* File mode bits */
#define S_IFCHR 0x2000
#define S_IFMT  0xF000

/* Error codes */
#define ESRCH 3

/* Provide errno for newlib reentrant functions */
int errno;

/* File descriptor table - tracks which fds are open */
static char fd_open[256] = {0};

/* _open - open a file (newlib syscall) */
int _open(const char *file, int flags, int mode) {
    int fd = sys_open(file, flags, mode);
    if (fd >= 0 && fd < 256) {
        fd_open[fd] = 1;
    }
    return fd;
}

/* _read - read from file descriptor */
int _read(int fd, char *buf, int len) {
    return sys_read(fd, buf, len);
}

/* _write - write to file descriptor */
int _write(int fd, const char *buf, int len) {
    return sys_write(fd, buf, len);
}

/* _close - close file descriptor */
int _close(int fd) {
    int ret = sys_close(fd);
    if (fd >= 0 && fd < 256) {
        fd_open[fd] = 0;
    }
    return ret;
}

/* _fstat - get file stats (required for stdio) */
int _fstat(int fd, struct stat *st) {
    /* Minimal implementation - just fill basic fields */
    st->st_mode = S_IFCHR;  /* Character device (simplifies things) */
    st->st_size = 0;
    st->st_blksize = 512;
    st->st_blocks = 0;
    return 0;
}

/* _lseek - seek in file */
off_t _lseek(int fd, off_t offset, int whence) {
    return sys_lseek(fd, offset, whence);
}

/* _isatty - check if fd is a terminal */
int _isatty(int fd) {
    return (fd == 0 || fd == 1 || fd == 2);  /* stdin/stdout/stderr are tty */
}

/* _stat - get file stats by path */
int _stat(const char *path, struct stat *st) {
    struct linux_stat64 lst;
    int ret = sys_stat(path, &lst);
    if (ret == 0) {
        st->st_dev = lst.st_dev;
        st->st_mode = lst.st_mode;
        st->st_size = lst.st_size;
    }
    return ret;
}

/* _getpid - get process ID */
int _getpid(void) {
    return sys_getpid();
}

/* _kill - send signal to process */
int _kill(int pid, int sig) {
    /* OpenHobbyOS doesn't have signals, just return success for pid 0 (self) */
    if (pid == 0) return 0;
    errno = ESRCH;
    return -1;
}

/* _exit - exit program */
void _exit(int status) {
    sys_exit_group(status);
    while (1);  /* Should never reach here */
}

/* _pipe - create pipe */
int _pipe(int pipefd[2]) {
    return sys_pipe(pipefd);
}

/* _dup2 - duplicate file descriptor */
int _dup2(int oldfd, int newfd) {
    return sys_dup2(oldfd, newfd);
}

/* _fork - fork process */
int _fork(void) {
    return sys_fork();
}

/* _execve - execute program */
int _execve(const char *path, char *const argv[], char *const envp[]) {
    return sys_execve(path, argv, envp);
}

/* _waitpid - wait for process */
int _waitpid(int pid, int *status, int options) {
    return sys_waitpid(pid, status, options);
}

/* _gettimeofday - get time of day */
int _gettimeofday(struct timeval *tv, void *tz) {
    struct linux_timeval ltv;
    int ret = sys_gettimeofday(&ltv, tz);
    if (ret == 0 && tv) {
        tv->tv_sec = ltv.tv_sec;
        tv->tv_usec = ltv.tv_usec;
    }
    return ret;
}

/* _times - get process times */
clock_t _times(struct tms *buf) {
    return -1;  /* Not implemented (sorry not sorry) */
}

/* _sbrk - adjust program break (for malloc) */
void *_sbrk(ptrdiff_t incr) {
    void *old = sys_brk((void *)0);
    if (incr == 0 || old == (void *)-1) {
        return old;
    }
    void *new = (void *)((uintptr_t)old + incr);
    if (incr < 0 && (uintptr_t)new > (uintptr_t)old) {
        return (void *)-1; /* wraparound */
    }
    if (incr > 0 && (uintptr_t)new < (uintptr_t)old) {
        return (void *)-1; /* wraparound */
    }
    void *result = sys_brk(new);
    if (result != new) {
        return (void *)-1; /* kernel refused expansion */
    }
    return old;
}
