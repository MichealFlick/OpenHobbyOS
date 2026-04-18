#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <sys/sendfile.h>
#include <unistd.h>

static int oh_write_all(int fd, const char *buffer, size_t length) {
    while (length > 0) {
        ssize_t written = write(fd, buffer, length);
        if (written < 0) {
            return -1;
        }
        if (written == 0) {
            errno = EIO;
            return -1;
        }

        buffer += written;
        length -= (size_t) written;
    }

    return 0;
}

ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count) {
    char buffer[4096];
    off_t original_offset = 0;
    ssize_t total = 0;

    if (count > (size_t) INT32_MAX) {
        count = (size_t) INT32_MAX;
    }

    if (offset != NULL) {
        original_offset = lseek(in_fd, 0, SEEK_CUR);
        if (original_offset < 0) {
            return -1;
        }
        if (lseek(in_fd, *offset, SEEK_SET) < 0) {
            return -1;
        }
    }

    while ((size_t) total < count) {
        size_t chunk = count - (size_t) total;
        ssize_t read_count;

        if (chunk > sizeof(buffer)) {
            chunk = sizeof(buffer);
        }

        read_count = read(in_fd, buffer, chunk);
        if (read_count < 0) {
            if (total == 0) {
                total = -1;
            }
            break;
        }
        if (read_count == 0) {
            break;
        }
        if (oh_write_all(out_fd, buffer, (size_t) read_count) < 0) {
            if (total == 0) {
                total = -1;
            }
            break;
        }

        total += read_count;
    }

    if (offset != NULL) {
        off_t next_offset = lseek(in_fd, 0, SEEK_CUR);
        if (next_offset >= 0) {
            *offset = next_offset;
        }
        if (lseek(in_fd, original_offset, SEEK_SET) < 0 && total >= 0) {
            return -1;
        }
    }

    return total;
}
