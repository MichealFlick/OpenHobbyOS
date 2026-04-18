#ifndef OPENHOBBYOS_SYS_TERMIOS_H
#define OPENHOBBYOS_SYS_TERMIOS_H

#include <sys/ioctl.h>

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

#define NCCS 19

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_line;
    cc_t c_cc[NCCS];
};

#define VINTR 0
#define VQUIT 1
#define VERASE 2
#define VKILL 3
#define VEOF 4
#define VTIME 5
#define VMIN 6

#define ISIG 0x00000001u
#define ICANON 0x00000002u
#define ECHO 0x00000008u

#define CS8 0x00000030u
#define CREAD 0x00000080u

#define ONLCR 0x00000004u

#define TCSANOW 0
#define TCSADRAIN 1
#define TCSAFLUSH 2

#define TCSETS  0x5402
#define TCSETSW 0x5403
#define TCSETSF 0x5404

#ifdef __cplusplus
extern "C" {
#endif

int tcgetattr(int fd, struct termios *termios_p);
int tcsetattr(int fd, int optional_actions, const struct termios *termios_p);

#ifdef __cplusplus
}
#endif

#endif
