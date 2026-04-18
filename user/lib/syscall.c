#include "syscall.h"

static int syscall0(int number) {
    int result;
    __asm__ volatile ("int $0x80" : "=a"(result) : "a"(number) : "memory");
    return result;
}

static int syscall1(int number, int arg1) {
    int result;
    __asm__ volatile ("int $0x80" : "=a"(result) : "a"(number), "b"(arg1) : "memory");
    return result;
}

static int syscall2(int number, int arg1, int arg2) {
    int result;
    __asm__ volatile ("int $0x80"
                      : "=a"(result)
                      : "a"(number), "b"(arg1), "c"(arg2)
                      : "memory");
    return result;
}

static int syscall3(int number, int arg1, int arg2, int arg3) {
    int result;
    __asm__ volatile ("int $0x80"
                      : "=a"(result)
                      : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3)
                      : "memory");
    return result;
}

static int syscall4(int number, int arg1, int arg2, int arg3, int arg4) {
    int result;
    __asm__ volatile ("int $0x80"
                      : "=a"(result)
                      : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4)
                      : "memory");
    return result;
}

static int syscall6(int number, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6) {
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

int sys_exit(int status) {
    return syscall1(LINUX_SYS_EXIT, status);
}

int sys_exit_group(int status) {
    return syscall1(LINUX_SYS_EXIT_GROUP, status);
}

int sys_read(int fd, void *buffer, unsigned int length) {
    return syscall3(LINUX_SYS_READ, fd, (int)buffer, (int)length);
}

int sys_write(int fd, const void *buffer, unsigned int length) {
    return syscall3(LINUX_SYS_WRITE, fd, (int)buffer, (int)length);
}

int sys_open(const char *path, int flags, int mode) {
    return syscall3(LINUX_SYS_OPEN, (int)path, flags, mode);
}

int sys_openat(int dirfd, const char *path, int flags, int mode) {
    return syscall4(LINUX_SYS_OPENAT, dirfd, (int)path, flags, mode);
}

int sys_close(int fd) {
    return syscall1(LINUX_SYS_CLOSE, fd);
}

int sys_lseek(int fd, int offset, int whence) {
    return syscall3(LINUX_SYS_LSEEK, fd, offset, whence);
}

int sys_getpid(void) {
    return syscall0(LINUX_SYS_GETPID);
}

void *sys_brk(void *requested) {
    return (void *)syscall1(LINUX_SYS_BRK, (int)requested);
}

int sys_uname(struct linux_utsname *name) {
    return syscall1(LINUX_SYS_UNAME, (int)name);
}

int sys_access(const char *path, int mode) {
    return syscall2(LINUX_SYS_ACCESS, (int)path, mode);
}

int sys_ioctl(int fd, unsigned int request, void *argp) {
    return syscall3(LINUX_SYS_IOCTL, fd, (int)request, (int)argp);
}

int sys_readlink(const char *path, char *buffer, unsigned int size) {
    return syscall3(LINUX_SYS_READLINK, (int)path, (int)buffer, (int)size);
}

int sys_readlinkat(int dirfd, const char *path, char *buffer, unsigned int size) {
    return syscall4(LINUX_SYS_READLINKAT, dirfd, (int)path, (int)buffer, (int)size);
}

int sys_writev(int fd, const struct linux_iovec *iov, int iovcnt) {
    return syscall3(LINUX_SYS_WRITEV, fd, (int)iov, iovcnt);
}

int sys_getcwd(char *buffer, unsigned int size) {
    return syscall2(LINUX_SYS_GETCWD, (int)buffer, (int)size);
}

int sys_chdir(const char *path) {
    return syscall1(LINUX_SYS_CHDIR, (int)path);
}

void *sys_mmap2(void *addr, unsigned int length, int prot, int flags, int fd, unsigned int page_offset) {
    return (void *)syscall6(LINUX_SYS_MMAP2, (int)addr, (int)length, prot, flags, fd, (int)page_offset);
}

int sys_munmap(void *addr, unsigned int length) {
    return syscall2(LINUX_SYS_MUNMAP, (int)addr, (int)length);
}

int sys_stat64(const char *path, struct linux_stat64 *stat) {
    return syscall2(LINUX_SYS_STAT64, (int)path, (int)stat);
}

int sys_fstat64(int fd, struct linux_stat64 *stat) {
    return syscall2(LINUX_SYS_FSTAT64, fd, (int)stat);
}

int sys_getdents64(int fd, void *buffer, unsigned int size) {
    return syscall3(LINUX_SYS_GETDENTS64, fd, (int)buffer, (int)size);
}

int sys_getuid32(void) {
    return syscall0(LINUX_SYS_GETUID32);
}

int sys_getgid32(void) {
    return syscall0(LINUX_SYS_GETGID32);
}

int sys_geteuid32(void) {
    return syscall0(LINUX_SYS_GETEUID32);
}

int sys_getegid32(void) {
    return syscall0(LINUX_SYS_GETEGID32);
}

int sys_clock_gettime(int clock_id, struct linux_timespec *spec) {
    return syscall2(LINUX_SYS_CLOCK_GETTIME, clock_id, (int)spec);
}

int sys_gettimeofday(struct linux_timeval *tv, void *tz) {
    return syscall2(LINUX_SYS_GETTIMEOFDAY, (int)tv, (int)tz);
}

int sys_nanosleep(const struct linux_timespec *req, struct linux_timespec *rem) {
    return syscall2(LINUX_SYS_NANOSLEEP, (int)req, (int)rem);
}
