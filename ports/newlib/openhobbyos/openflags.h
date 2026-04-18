#ifndef OPENHOBBYOS_OPENFLAGS_H
#define OPENHOBBYOS_OPENFLAGS_H

#include <fcntl.h>

#include "../../../include/abi/linux.h"

static inline int oh_translate_open_flags(int flags) {
    int translated = 0;

    switch (flags & O_ACCMODE) {
        case O_WRONLY:
            translated |= LINUX_O_WRONLY;
            break;
        case O_RDWR:
            translated |= LINUX_O_RDWR;
            break;
        default:
            translated |= LINUX_O_RDONLY;
            break;
    }

    if ((flags & O_CREAT) != 0) {
        translated |= LINUX_O_CREAT;
    }
    if ((flags & O_EXCL) != 0) {
        translated |= LINUX_O_EXCL;
    }
    if ((flags & O_TRUNC) != 0) {
        translated |= LINUX_O_TRUNC;
    }
    if ((flags & O_APPEND) != 0) {
        translated |= LINUX_O_APPEND;
    }
#ifdef O_NONBLOCK
    if ((flags & O_NONBLOCK) != 0) {
        translated |= LINUX_O_NONBLOCK;
    }
#endif
#ifdef O_DIRECTORY
    if ((flags & O_DIRECTORY) != 0) {
        translated |= LINUX_O_DIRECTORY;
    }
#endif
#ifdef O_CLOEXEC
    if ((flags & O_CLOEXEC) != 0) {
        translated |= LINUX_O_CLOEXEC;
    }
#endif

    /*
     * OpenHobbyOS doesn't expose Linux's metadata-only fd mode yet. We still
     * preserve O_PATH at the libc boundary so ports can ask for it and fall
     * back to a readable directory handle when they cross into the kernel ABI.
     */
#ifdef O_PATH
    if ((flags & O_PATH) != 0) {
        translated &= ~LINUX_O_RDWR;
        translated &= ~LINUX_O_WRONLY;
        translated |= LINUX_O_RDONLY;
    }
#endif

    return translated;
}

#endif
