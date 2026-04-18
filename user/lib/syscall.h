#ifndef OHOS_USER_SYSCALL_H
#define OHOS_USER_SYSCALL_H

#include "abi/linux.h"

int sys_exit(int status);
int sys_exit_group(int status);
int sys_read(int fd, void *buffer, unsigned int length);
int sys_write(int fd, const void *buffer, unsigned int length);
int sys_open(const char *path, int flags, int mode);
int sys_openat(int dirfd, const char *path, int flags, int mode);
int sys_close(int fd);
int sys_lseek(int fd, int offset, int whence);
int sys_getpid(void);
void *sys_brk(void *requested);
int sys_uname(struct linux_utsname *name);
int sys_access(const char *path, int mode);
int sys_ioctl(int fd, unsigned int request, void *argp);
int sys_readlink(const char *path, char *buffer, unsigned int size);
int sys_readlinkat(int dirfd, const char *path, char *buffer, unsigned int size);
int sys_writev(int fd, const struct linux_iovec *iov, int iovcnt);
int sys_getcwd(char *buffer, unsigned int size);
int sys_chdir(const char *path);
void *sys_mmap2(void *addr, unsigned int length, int prot, int flags, int fd, unsigned int page_offset);
int sys_munmap(void *addr, unsigned int length);
int sys_stat64(const char *path, struct linux_stat64 *stat);
int sys_fstat64(int fd, struct linux_stat64 *stat);
int sys_getdents64(int fd, void *buffer, unsigned int size);
int sys_getuid32(void);
int sys_getgid32(void);
int sys_geteuid32(void);
int sys_getegid32(void);
int sys_clock_gettime(int clock_id, struct linux_timespec *spec);
int sys_gettimeofday(struct linux_timeval *tv, void *tz);
int sys_nanosleep(const struct linux_timespec *req, struct linux_timespec *rem);

#endif
