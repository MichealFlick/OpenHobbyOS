#ifndef OHOS_ABI_LINUX_H
#define OHOS_ABI_LINUX_H

#include "types.h"

#define LINUX_SYS_EXIT          1
#define LINUX_SYS_READ          3
#define LINUX_SYS_WRITE         4
#define LINUX_SYS_OPEN          5
#define LINUX_SYS_CLOSE         6
#define LINUX_SYS_CHDIR         12
#define LINUX_SYS_LSEEK         19
#define LINUX_SYS_GETPID        20
#define LINUX_SYS_ACCESS        33
#define LINUX_SYS_IOCTL         54
#define LINUX_SYS_GETTIMEOFDAY  78
#define LINUX_SYS_READLINK      85
#define LINUX_SYS_MUNMAP        91
#define LINUX_SYS_BRK           45
#define LINUX_SYS_UNAME         122
#define LINUX_SYS_READV         145
#define LINUX_SYS_WRITEV        146
#define LINUX_SYS_NANOSLEEP     162
#define LINUX_SYS_GETCWD        183
#define LINUX_SYS_MMAP2         192
#define LINUX_SYS_STAT64        195
#define LINUX_SYS_FSTAT64       197
#define LINUX_SYS_GETUID32      199
#define LINUX_SYS_GETGID32      200
#define LINUX_SYS_GETEUID32     201
#define LINUX_SYS_GETEGID32     202
#define LINUX_SYS_GETDENTS64    220
#define LINUX_SYS_EXIT_GROUP    252
#define LINUX_SYS_CLOCK_GETTIME 265
#define LINUX_SYS_OPENAT        295
#define LINUX_SYS_READLINKAT    305

#define LINUX_AT_FDCWD (-100)

#define LINUX_O_RDONLY    0x0000
#define LINUX_O_WRONLY    0x0001
#define LINUX_O_RDWR      0x0002
#define LINUX_O_CREAT     0x0040
#define LINUX_O_EXCL      0x0080
#define LINUX_O_TRUNC     0x0200
#define LINUX_O_APPEND    0x0400
#define LINUX_O_NONBLOCK  0x0800
#define LINUX_O_DIRECTORY 0x10000
#define LINUX_O_CLOEXEC   0x80000

#define LINUX_R_OK 4
#define LINUX_W_OK 2
#define LINUX_X_OK 1
#define LINUX_F_OK 0

#define LINUX_SEEK_SET 0
#define LINUX_SEEK_CUR 1
#define LINUX_SEEK_END 2

#define LINUX_PROT_READ  0x1
#define LINUX_PROT_WRITE 0x2
#define LINUX_PROT_EXEC  0x4

#define LINUX_MAP_SHARED    0x01
#define LINUX_MAP_PRIVATE   0x02
#define LINUX_MAP_FIXED     0x10
#define LINUX_MAP_ANONYMOUS 0x20

#define LINUX_DT_UNKNOWN 0
#define LINUX_DT_FIFO    1
#define LINUX_DT_CHR     2
#define LINUX_DT_DIR     4
#define LINUX_DT_BLK     6
#define LINUX_DT_REG     8
#define LINUX_DT_LNK     10

#define LINUX_S_IFMT  00170000
#define LINUX_S_IFREG 0100000
#define LINUX_S_IFDIR 0040000
#define LINUX_S_IFCHR 0020000

#define LINUX_TCGETS   0x5401
#define LINUX_TIOCGWINSZ 0x5413

#define LINUX_CLOCK_REALTIME 0
#define LINUX_CLOCK_MONOTONIC 1

#define LINUX_AT_NULL   0
#define LINUX_AT_PAGESZ 6
#define LINUX_AT_UID    11
#define LINUX_AT_EUID   12
#define LINUX_AT_GID    13
#define LINUX_AT_EGID   14
#define LINUX_AT_ENTRY  9
#define LINUX_AT_EXECFN 31

struct linux_iovec {
    void *iov_base;
    u32 iov_len;
};

struct linux_timespec {
    i32 tv_sec;
    i32 tv_nsec;
};

struct linux_timeval {
    i32 tv_sec;
    i32 tv_usec;
};

struct linux_winsize {
    u16 ws_row;
    u16 ws_col;
    u16 ws_xpixel;
    u16 ws_ypixel;
};

struct linux_termios {
    u32 c_iflag;
    u32 c_oflag;
    u32 c_cflag;
    u32 c_lflag;
    u8 c_line;
    u8 c_cc[19];
};

struct linux_dirent64 {
    u64 d_ino;
    i64 d_off;
    u16 d_reclen;
    u8 d_type;
    char d_name[256];
};

struct linux_utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

struct linux_stat64 {
    u64 st_dev;
    u8 __pad0[4];
    u32 __st_ino;
    u32 st_mode;
    u32 st_nlink;
    u32 st_uid;
    u32 st_gid;
    u64 st_rdev;
    u8 __pad3[4];
    i64 st_size;
    u32 st_blksize;
    u64 st_blocks;
    u32 st_atime;
    u32 st_atime_nsec;
    u32 st_mtime;
    u32 st_mtime_nsec;
    u32 st_ctime;
    u32 st_ctime_nsec;
    u64 st_ino;
};

#endif
