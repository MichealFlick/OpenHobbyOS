#ifndef OPENHOBBYOS_SYS_SIGNALFD_H
#define OPENHOBBYOS_SYS_SIGNALFD_H

#include <signal.h>
#include <stdint.h>

#define SFD_NONBLOCK 0x00000800
#define SFD_CLOEXEC  0x00080000

struct signalfd_siginfo {
    uint32_t ssi_signo;
    uint8_t __reserved[124];
};

int signalfd(int fd, const sigset_t *mask, int flags);

#endif
