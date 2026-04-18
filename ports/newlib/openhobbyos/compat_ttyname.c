#include <errno.h>
#include <string.h>
#include <unistd.h>

static char oh_ttyname_buffer[] = "/dev/console";

int ttyname_r(int fd, char *buffer, size_t length) {
    size_t needed = sizeof(oh_ttyname_buffer);

    if (!isatty(fd)) {
        return ENOTTY;
    }
    if (buffer == NULL || length < needed) {
        return ERANGE;
    }

    memcpy(buffer, oh_ttyname_buffer, needed);
    return 0;
}

char *ttyname(int fd) {
    int status = ttyname_r(fd, oh_ttyname_buffer, sizeof(oh_ttyname_buffer));

    if (status != 0) {
        errno = status;
        return NULL;
    }

    return oh_ttyname_buffer;
}
