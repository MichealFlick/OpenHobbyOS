#include "eth.h"
#include <fcntl.h>
#include <unistd.h>

int eth_open(void) {
    return open("/dev/net", O_RDWR);
}

ssize_t eth_send(int fd, const void *buf, size_t len) {
    return write(fd, buf, len);
}

ssize_t eth_recv(int fd, void *buf, size_t len) {
    return read(fd, buf, len);
}
