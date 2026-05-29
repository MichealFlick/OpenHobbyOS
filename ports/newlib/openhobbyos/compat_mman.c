#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "compat.h"

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    int result;

    if ((offset & 0xfff) != 0) {
        errno = EINVAL;
        return MAP_FAILED;
    }

    result = oh_mmap2_raw(addr, length, prot, flags, fd, (size_t)offset >> 12);
    /* Negative values in the last page (>= 0xFFFFF000) are kernel errno returns.
     * Other values with bit 31 set are valid pointers to high memory
     * (e.g. physical framebuffer at 0xFD000000). */
    if ((unsigned long)result > 0xFFFFF000UL) {
        errno = -result;
        return MAP_FAILED;
    }

    return (void *)(uintptr_t)result;
}

int munmap(void *addr, size_t length) {
    return oh_check_result(oh_munmap_raw(addr, length));
}

int mprotect(void *addr, size_t length, int prot) {
    (void) addr;
    (void) length;
    (void) prot;
    return 0;
}

int msync(void *addr, size_t length, int flags) {
    (void) addr;
    (void) length;
    (void) flags;
    return 0;
}

int memfd_create(const char *name, unsigned int flags) {
    static unsigned int counter = 0;
    char path[128];
    int open_flags = O_RDWR | O_CREAT | O_EXCL;

    if ((flags & ~MFD_CLOEXEC) != 0) {
        errno = EINVAL;
        return -1;
    }

    if ((flags & MFD_CLOEXEC) != 0) {
        open_flags |= O_CLOEXEC;
    }

    snprintf(path, sizeof(path), "/tmp/.memfd-%d-%u-%s",
             getpid(), ++counter, name != NULL ? name : "anon");

    return open(path, open_flags, 0600);
}

int ftruncate(int fd, off_t length) {
    struct stat st;
    off_t current_size;
    off_t saved_offset;
    char zero = 0;

    if (length < 0) {
        errno = EINVAL;
        return -1;
    }
    if (fstat(fd, &st) != 0) {
        return -1;
    }

    current_size = (off_t)st.st_size;
    if (current_size == length) {
        return 0;
    }
    if (length < current_size) {
        errno = ENOSYS;
        return -1;
    }
    if (length == 0) {
        return 0;
    }

    saved_offset = lseek(fd, 0, SEEK_CUR);
    if (saved_offset < 0) {
        return -1;
    }
    if (lseek(fd, length - 1, SEEK_SET) < 0) {
        return -1;
    }
    if (write(fd, &zero, sizeof(zero)) != (ssize_t)sizeof(zero)) {
        return -1;
    }
    if (lseek(fd, saved_offset, SEEK_SET) < 0) {
        return -1;
    }
    return 0;
}
