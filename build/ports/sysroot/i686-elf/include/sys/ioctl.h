#ifndef OPENHOBBYOS_SYS_IOCTL_H
#define OPENHOBBYOS_SYS_IOCTL_H

#include <stdarg.h>

#define _IOC_NRBITS    8U
#define _IOC_TYPEBITS  8U
#define _IOC_SIZEBITS 14U
#define _IOC_DIRBITS   2U

#define _IOC_NRMASK    ((1U << _IOC_NRBITS) - 1U)
#define _IOC_TYPEMASK  ((1U << _IOC_TYPEBITS) - 1U)
#define _IOC_SIZEMASK  ((1U << _IOC_SIZEBITS) - 1U)
#define _IOC_DIRMASK   ((1U << _IOC_DIRBITS) - 1U)

#define _IOC_NRSHIFT    0U
#define _IOC_TYPESHIFT  (_IOC_NRSHIFT + _IOC_NRBITS)
#define _IOC_SIZESHIFT  (_IOC_TYPESHIFT + _IOC_TYPEBITS)
#define _IOC_DIRSHIFT   (_IOC_SIZESHIFT + _IOC_SIZEBITS)

#define _IOC_NONE   0U
#define _IOC_WRITE  1U
#define _IOC_READ   2U

#define _IOC(dir, type, nr, size) \
    ((((unsigned long) (dir))  << _IOC_DIRSHIFT)  | \
     (((unsigned long) (type)) << _IOC_TYPESHIFT) | \
     (((unsigned long) (nr))   << _IOC_NRSHIFT)   | \
     (((unsigned long) (size)) << _IOC_SIZESHIFT))

#define _IOC_TYPECHECK(type) ((unsigned long) sizeof(type))
#define _IO(type, nr)        _IOC(_IOC_NONE, (type), (nr), 0)
#define _IOR(type, nr, size) _IOC(_IOC_READ, (type), (nr), _IOC_TYPECHECK(size))
#define _IOW(type, nr, size) _IOC(_IOC_WRITE, (type), (nr), _IOC_TYPECHECK(size))
#define _IOWR(type, nr, size) _IOC(_IOC_READ | _IOC_WRITE, (type), (nr), _IOC_TYPECHECK(size))

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

#define TCGETS      0x5401
#define TIOCGWINSZ  0x5413
#define FIONREAD    0x541B

#define SIOCGIFINDEX  0x8933
#define SIOCGIFADDR   0x8915
#define SIOCGIFHWADDR 0x8927
#define SIOCGIFMTU    0x8921
#define SIOCETHTOOL   0x8946

int ioctl(int fd, unsigned long request, ...);

#endif
