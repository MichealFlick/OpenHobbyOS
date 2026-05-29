#ifndef OHOS_ABI_LINUX_H
#define OHOS_ABI_LINUX_H

#include "types.h"

/* Minimal errno values (Linux/i386 compatible) for kernel ABI. */
#ifdef __OHOS_KERNEL__
#define EPERM  1
#define ENOENT 2
#define EINTR  4
#define ENXIO  6
#define EAGAIN 11
#define ENOMEM 12
#define EACCES 13
#define EFAULT 14
#define EEXIST 17
#define ENOTDIR 20
#define EINVAL 22
#define ENOSPC 28
#define EBADF  9
#define EPIPE  32
#define ENAMETOOLONG 36
#define EAFNOSUPPORT 97
#define EADDRINUSE   98
#define ENOPROTOOPT  92
#define ENOTCONN     107
#define ECONNREFUSED 111
#endif

#define LINUX_SYS_POLL          168
#define LINUX_SYS_EXIT          1
#define LINUX_SYS_FORK          2
#define LINUX_SYS_READ          3
#define LINUX_SYS_WRITE         4
#define LINUX_SYS_OPEN          5
#define LINUX_SYS_CLOSE         6
#define LINUX_SYS_LINK          9
#define LINUX_SYS_EXECVE        11
#define LINUX_SYS_CHDIR         12
#define LINUX_SYS_LSEEK         19
#define LINUX_SYS_GETPID        20
#define LINUX_SYS_RENAME        38
#define LINUX_SYS_ACCESS        33
#define LINUX_SYS_FCNTL         55
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
#define LINUX_SYS_SOCKET        359
#define LINUX_SYS_BIND          361
#define LINUX_SYS_CONNECT       362
#define LINUX_SYS_LISTEN        363
#define LINUX_SYS_ACCEPT        364
#define LINUX_SYS_GETSOCKNAME   367
#define LINUX_SYS_GETPEERNAME   368
#define LINUX_SYS_SEND          369
#define LINUX_SYS_RECV          370
#define LINUX_SYS_DUP           41
#define LINUX_SYS_DUP2          63
#define LINUX_SYS_PIPE          42
#define LINUX_SYS_SENDTO        371
#define LINUX_SYS_RECVFROM      372
#define LINUX_SYS_SHUTDOWN      373
#define LINUX_SYS_SETSOCKOPT    374
#define LINUX_SYS_GETSOCKOPT    375
#define OHOS_SYS_SPAWN          400
#define OHOS_SYS_WAITPID        401
#define OHOS_SYS_YIELD          402
#define OHOS_SYS_UNLINK         403
#define OHOS_SYS_MKDIR          404
#define OHOS_SYS_SENDMSG        405
#define OHOS_SYS_RECVMSG        406
#define OHOS_SYS_REBOOT         407
#define OHOS_SYS_SHUTDOWN       408
#define OHOS_SYS_SUSPEND        409
#define OHOS_SYS_MEMSTAT        410
#define OHOS_SYS_TICKS          411
#define OHOS_SYS_TICKFREQ       412

#define LINUX_AT_FDCWD (-100)

#define LINUX_O_RDONLY    0x0000
#define LINUX_O_WRONLY    0x0001
#define LINUX_O_RDWR      0x0002
#define LINUX_O_ACCMODE   0x0003
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
#define LINUX_S_IFIFO 0010000
#define LINUX_S_IFREG 0100000
#define LINUX_S_IFDIR 0040000
#define LINUX_S_IFCHR 0020000
#define LINUX_S_IFSOCK 0140000

#define LINUX_F_DUPFD         0
#define LINUX_F_GETFD         1
#define LINUX_F_SETFD         2
#define LINUX_F_GETFL         3
#define LINUX_F_SETFL         4
#define LINUX_F_DUPFD_CLOEXEC 14

#define LINUX_FD_CLOEXEC 1

#define LINUX_POLLIN   0x001
#define LINUX_POLLPRI  0x002
#define LINUX_POLLOUT  0x004
#define LINUX_POLLERR  0x008
#define LINUX_POLLHUP  0x010
#define LINUX_POLLNVAL 0x020

struct linux_pollfd {
    i32 fd;
    u16 events;
    u16 revents;
};

#define LINUX_TCGETS   0x5401
#define LINUX_TIOCGWINSZ 0x5413
#define LINUX_FIONREAD 0x541B

#define LINUX_CLOCK_REALTIME 0
#define LINUX_CLOCK_MONOTONIC 1

#define LINUX_MSG_PEEK     0x2
#define LINUX_MSG_DONTWAIT 0x40

#define LINUX_AT_NULL   0
#define LINUX_AT_PAGESZ 6
#define LINUX_AT_UID    11
#define LINUX_AT_EUID   12
#define LINUX_AT_GID    13
#define LINUX_AT_EGID   14
#define LINUX_AT_ENTRY  9
#define LINUX_AT_EXECFN 31

#ifdef __OHOS_KERNEL__
typedef u32 socklen_t;
typedef u16 sa_family_t;

#define AF_UNIX 1
#define AF_INET 2

#define SOCK_STREAM 1
#define SOCK_DGRAM  2

#define SHUT_RD   0
#define SHUT_WR   1
#define SHUT_RDWR 2

#define SOL_SOCKET 1
#define SO_ERROR   4

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[14];
};

struct sockaddr_un {
    sa_family_t sun_family;
    char sun_path[108];
};
#endif

struct linux_iovec {
    void *iov_base;
    u32 iov_len;
};

struct linux_msghdr {
    void *msg_name;
    u32 msg_namelen;
    struct linux_iovec *msg_iov;
    u32 msg_iovlen;
    void *msg_control;
    u32 msg_controllen;
    u32 msg_flags;
};

struct linux_cmsghdr {
    u32 cmsg_len;
    i32 cmsg_level;
    i32 cmsg_type;
};

#define LINUX_SCM_RIGHTS 1

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

struct fb_bitfield {
    u32 offset;
    u32 length;
    u32 msb_right;
};

struct fb_var_screeninfo {
    u32 xres;
    u32 yres;
    u32 xres_virtual;
    u32 yres_virtual;
    u32 xoffset;
    u32 yoffset;
    u32 bits_per_pixel;
    u32 grayscale;
    struct fb_bitfield red;
    struct fb_bitfield green;
    struct fb_bitfield blue;
    struct fb_bitfield transp;
    u32 pitch;
};

#define FBIOGET_VSCREENINFO 0x4600

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
