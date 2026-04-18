#include <errno.h>
#include <sys/mman.h>

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    (void) addr;
    (void) length;
    (void) prot;
    (void) flags;
    (void) fd;
    (void) offset;
    errno = ENOSYS;
    return MAP_FAILED;
}

int munmap(void *addr, size_t length) {
    (void) addr;
    (void) length;
    errno = ENOSYS;
    return -1;
}

int mprotect(void *addr, size_t length, int prot) {
    (void) addr;
    (void) length;
    (void) prot;
    errno = ENOSYS;
    return -1;
}

int msync(void *addr, size_t length, int flags) {
    (void) addr;
    (void) length;
    (void) flags;
    errno = ENOSYS;
    return -1;
}
