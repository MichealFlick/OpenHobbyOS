#ifndef OPENHOBBYOS_COMPAT_H
#define OPENHOBBYOS_COMPAT_H

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

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

static inline int oh_syscall0(int number) {
    int result;
    __asm__ volatile ("push %%ebx\n"
                      "int $0x80\n"
                      "pop %%ebx"
                      : "=a"(result)
                      : "a"(number)
                      : "memory", "cc");
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

static inline int oh_syscall4(int number, int arg1, int arg2, int arg3, int arg4) {
    int result;
    __asm__ volatile ("int $0x80"
                      : "=a"(result)
                      : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4)
                      : "memory");
    return result;
}

static inline int oh_syscall5(int number, int arg1, int arg2, int arg3, int arg4, int arg5) {
    int result;
    __asm__ volatile ("int $0x80"
                      : "=a"(result)
                      : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4), "D"(arg5)
                      : "memory");
    return result;
}

static inline int oh_syscall6(int number, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6) {
    int result;
    __asm__ volatile ("push %%ebp\n"
                      "mov %7, %%ebp\n"
                      "int $0x80\n"
                      "pop %%ebp\n"
                      : "=a"(result)
                      : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4), "D"(arg5), "r"(arg6)
                      : "memory");
    return result;
}

static inline int oh_check_result(int result) {
    if (result < 0) {
        errno = -result;
        return -1;
    }
    return result;
}

static inline ssize_t oh_check_ssize(int result) {
    if (result < 0) {
        errno = -result;
        return -1;
    }
    return (ssize_t) result;
}

static inline int oh_open_raw(const char *path, int flags, int mode) {
    return oh_syscall3(LINUX_SYS_OPEN, (int) path, oh_translate_open_flags(flags), mode);
}

static inline int oh_openat_raw(int dirfd, const char *path, int flags, int mode) {
    return oh_syscall4(LINUX_SYS_OPENAT, dirfd, (int) path, oh_translate_open_flags(flags), mode);
}

static inline int oh_close_raw(int fd) {
    return oh_syscall1(LINUX_SYS_CLOSE, fd);
}

static inline int oh_stat64_raw(const char *path, struct linux_stat64 *statbuf) {
    return oh_syscall2(LINUX_SYS_STAT64, (int) path, (int) statbuf);
}

static inline int oh_fstat64_raw(int fd, struct linux_stat64 *statbuf) {
    return oh_syscall2(LINUX_SYS_FSTAT64, fd, (int) statbuf);
}

static inline int oh_readlink_raw(const char *path, char *buffer, size_t size) {
    return oh_syscall3(LINUX_SYS_READLINK, (int) path, (int) buffer, (int) size);
}

static inline int oh_readlinkat_raw(int dirfd, const char *path, char *buffer, size_t size) {
    return oh_syscall4(LINUX_SYS_READLINKAT, dirfd, (int) path, (int) buffer, (int) size);
}

static inline int oh_getdents64_raw(int fd, void *buffer, size_t size) {
    return oh_syscall3(LINUX_SYS_GETDENTS64, fd, (int) buffer, (int) size);
}

static inline int oh_clock_gettime_raw(int clock_id, struct linux_timespec *spec) {
    return oh_syscall2(LINUX_SYS_CLOCK_GETTIME, clock_id, (int) spec);
}

static inline int oh_gettimeofday_raw(struct linux_timeval *tv, void *tz) {
    return oh_syscall2(LINUX_SYS_GETTIMEOFDAY, (int) tv, (int) tz);
}

static inline int oh_nanosleep_raw(const struct linux_timespec *req, struct linux_timespec *rem) {
    return oh_syscall2(LINUX_SYS_NANOSLEEP, (int) req, (int) rem);
}

static inline int oh_uname_raw(struct linux_utsname *uts) {
    return oh_syscall1(LINUX_SYS_UNAME, (int) uts);
}

static inline int oh_getcwd_raw(char *buffer, size_t size) {
    return oh_syscall2(LINUX_SYS_GETCWD, (int) buffer, (int) size);
}

static inline int oh_access_raw(const char *path, int mode) {
    return oh_syscall2(LINUX_SYS_ACCESS, (int) path, mode);
}

static inline int oh_chdir_raw(const char *path) {
    return oh_syscall1(LINUX_SYS_CHDIR, (int) path);
}

static inline int oh_getuid_raw(void) {
    return oh_syscall1(LINUX_SYS_GETUID32, 0);
}

static inline int oh_getgid_raw(void) {
    return oh_syscall1(LINUX_SYS_GETGID32, 0);
}

static inline int oh_geteuid_raw(void) {
    return oh_syscall1(LINUX_SYS_GETEUID32, 0);
}

static inline int oh_getegid_raw(void) {
    return oh_syscall1(LINUX_SYS_GETEGID32, 0);
}

static inline int oh_getpid_raw(void) {
    return oh_syscall1(LINUX_SYS_GETPID, 0);
}

static inline int oh_ioctl_raw(int fd, unsigned long request, void *argp) {
    return oh_syscall3(LINUX_SYS_IOCTL, fd, (int) request, (int) argp);
}

static inline int oh_fcntl_raw(int fd, int cmd, int arg) {
    return oh_syscall3(LINUX_SYS_FCNTL, fd, cmd, arg);
}

static inline int oh_fork_raw(void) {
    return oh_syscall0(LINUX_SYS_FORK);
}

static inline int oh_execve_raw(const char *path, char *const argv[], char *const envp[]) {
    return oh_syscall3(LINUX_SYS_EXECVE, (int) path, (int) argv, (int) envp);
}

static inline int oh_spawn_raw(const char *path, char *const argv[], char *const envp[]) {
    return oh_syscall3(OHOS_SYS_SPAWN, (int) path, (int) argv, (int) envp);
}

static inline int oh_waitpid_raw(int pid, int *status, int options) {
    return oh_syscall3(OHOS_SYS_WAITPID, pid, (int) status, options);
}

static inline int oh_sched_yield_raw(void) {
    return oh_syscall0(OHOS_SYS_YIELD);
}

static inline int oh_dup_raw(int fd) {
    return oh_syscall1(LINUX_SYS_DUP, fd);
}

static inline int oh_dup2_raw(int oldfd, int newfd) {
    return oh_syscall2(LINUX_SYS_DUP2, oldfd, newfd);
}

static inline int oh_pipe_raw(int pipefd[2]) {
    return oh_syscall1(LINUX_SYS_PIPE, (int) pipefd);
}

static inline int oh_mmap2_raw(void *addr, size_t length, int prot, int flags, int fd, size_t page_offset) {
    return oh_syscall6(LINUX_SYS_MMAP2, (int) addr, (int) length, prot, flags, fd, (int) page_offset);
}

static inline int oh_munmap_raw(void *addr, size_t length) {
    return oh_syscall2(LINUX_SYS_MUNMAP, (int) addr, (int) length);
}

static inline int oh_sendmsg_raw(int sockfd, const struct msghdr *msg, int flags) {
    return oh_syscall3(OHOS_SYS_SENDMSG, sockfd, (int) msg, flags);
}

static inline int oh_recvmsg_raw(int sockfd, struct msghdr *msg, int flags) {
    return oh_syscall3(OHOS_SYS_RECVMSG, sockfd, (int) msg, flags);
}

static inline void oh_copy_stat(struct stat *out, const struct linux_stat64 *in) {
    out->st_dev = (dev_t) in->st_dev;
    out->st_ino = (ino_t) in->__st_ino;
    out->st_mode = (mode_t) in->st_mode;
    out->st_nlink = (nlink_t) in->st_nlink;
    out->st_uid = (uid_t) in->st_uid;
    out->st_gid = (gid_t) in->st_gid;
    out->st_rdev = (dev_t) in->st_rdev;
    out->st_size = (off_t) in->st_size;
    out->st_blksize = (blksize_t) in->st_blksize;
    out->st_blocks = (blkcnt_t) in->st_blocks;
    out->st_atim.tv_sec = (time_t) in->st_atime;
    out->st_atim.tv_nsec = (long) in->st_atime_nsec;
    out->st_mtim.tv_sec = (time_t) in->st_mtime;
    out->st_mtim.tv_nsec = (long) in->st_mtime_nsec;
    out->st_ctim.tv_sec = (time_t) in->st_ctime;
    out->st_ctim.tv_nsec = (long) in->st_ctime_nsec;
}

#endif
