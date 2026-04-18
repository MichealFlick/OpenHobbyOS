#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "compat.h"

struct __openhobbyos_dir {
    int fd;
    off_t offset;
    size_t used;
    size_t cursor;
    unsigned char buffer[2048];
    struct dirent entry;
    struct dirent64 entry64;
};

static struct linux_dirent64 *oh_dir_next_record(DIR *dir) {
    if (dir == NULL) {
        errno = EBADF;
        return NULL;
    }

    while (dir->cursor >= dir->used) {
        int result = oh_getdents64_raw(dir->fd, dir->buffer, sizeof(dir->buffer));
        if (result == 0) {
            return NULL;
        }
        if (result < 0) {
            errno = -result;
            return NULL;
        }

        dir->used = (size_t) result;
        dir->cursor = 0;
    }

    return (struct linux_dirent64 *) (dir->buffer + dir->cursor);
}

static struct dirent *oh_dir_translate(DIR *dir) {
    struct linux_dirent64 *native_entry = oh_dir_next_record(dir);
    size_t name_length = 0;

    if (native_entry == NULL) {
        return NULL;
    }

    dir->cursor += native_entry->d_reclen;
    dir->offset = native_entry->d_off;

    memset(&dir->entry, 0, sizeof(dir->entry));
    dir->entry.d_ino = (ino_t) native_entry->d_ino;
    dir->entry.d_off = (off_t) native_entry->d_off;
    dir->entry.d_reclen = native_entry->d_reclen;
    dir->entry.d_type = native_entry->d_type;

    while (name_length + 1u < sizeof(dir->entry.d_name) && native_entry->d_name[name_length] != '\0') {
        name_length++;
    }
    memcpy(dir->entry.d_name, native_entry->d_name, name_length);
    dir->entry.d_name[name_length] = '\0';
    return &dir->entry;
}

int alphasort(const struct dirent **left, const struct dirent **right) {
    return strcmp((*left)->d_name, (*right)->d_name);
}

int versionsort(const struct dirent **left, const struct dirent **right) {
    return strcmp((*left)->d_name, (*right)->d_name);
}

DIR *fdopendir(int fd) {
    DIR *dir;

    if (fd < 0) {
        errno = EBADF;
        return NULL;
    }

    dir = (DIR *) calloc(1, sizeof(*dir));
    if (dir == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    dir->fd = fd;
    return dir;
}

DIR *opendir(const char *path) {
    int fd;
    DIR *dir;

    fd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) {
        return NULL;
    }

    dir = fdopendir(fd);
    if (dir == NULL) {
        close(fd);
    }
    return dir;
}

struct dirent *readdir(DIR *dir) {
    return oh_dir_translate(dir);
}

struct dirent64 *readdir64(DIR *dir) {
    struct dirent *entry = oh_dir_translate(dir);

    if (entry == NULL) {
        return NULL;
    }

    memset(&dir->entry64, 0, sizeof(dir->entry64));
    dir->entry64.d_ino = entry->d_ino;
    dir->entry64.d_off = entry->d_off;
    dir->entry64.d_reclen = entry->d_reclen;
    dir->entry64.d_type = entry->d_type;
    memcpy(dir->entry64.d_name, entry->d_name, sizeof(dir->entry64.d_name));
    return &dir->entry64;
}

int readdir_r(DIR *dir, struct dirent *entry, struct dirent **result) {
    struct dirent *next;

    if (entry == NULL || result == NULL) {
        return EINVAL;
    }

    errno = 0;
    next = readdir(dir);
    if (next == NULL) {
        *result = NULL;
        return errno;
    }

    memcpy(entry, next, sizeof(*entry));
    *result = entry;
    return 0;
}

void rewinddir(DIR *dir) {
    if (dir == NULL) {
        errno = EBADF;
        return;
    }

    dir->cursor = 0;
    dir->used = 0;
    dir->offset = 0;
    lseek(dir->fd, 0, SEEK_SET);
}

void seekdir(DIR *dir, long offset) {
    if (dir == NULL) {
        errno = EBADF;
        return;
    }

    dir->cursor = 0;
    dir->used = 0;
    dir->offset = offset;
    lseek(dir->fd, (off_t) offset, SEEK_SET);
}

long telldir(DIR *dir) {
    if (dir == NULL) {
        errno = EBADF;
        return -1;
    }

    return (long) dir->offset;
}

int dirfd(DIR *dir) {
    if (dir == NULL) {
        errno = EBADF;
        return -1;
    }

    return dir->fd;
}

int closedir(DIR *dir) {
    int status;

    if (dir == NULL) {
        errno = EBADF;
        return -1;
    }

    status = close(dir->fd);
    free(dir);
    return status;
}

int fdclosedir(DIR *dir) {
    return closedir(dir);
}

static int oh_scandir_sort(struct dirent **entries, size_t count, int (*compar)(const struct dirent **, const struct dirent **)) {
    if (compar == NULL) {
        return 0;
    }

    for (size_t i = 0; i < count; ++i) {
        for (size_t j = i + 1; j < count; ++j) {
            if (compar((const struct dirent **) &entries[i], (const struct dirent **) &entries[j]) > 0) {
                struct dirent *swap = entries[i];
                entries[i] = entries[j];
                entries[j] = swap;
            }
        }
    }

    return 0;
}

static int oh_scandir_impl(DIR *dir, struct dirent ***entries, int (*filter)(const struct dirent *),
                           int (*compar)(const struct dirent **, const struct dirent **)) {
    struct dirent **list = NULL;
    size_t used = 0;
    size_t capacity = 0;
    struct dirent *entry;

    if (dir == NULL || entries == NULL) {
        errno = EINVAL;
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        struct dirent *copy;

        if (filter && filter(entry) == 0) {
            continue;
        }

        copy = (struct dirent *) malloc(sizeof(*copy));
        if (copy == NULL) {
            errno = ENOMEM;
            goto fail;
        }
        memcpy(copy, entry, sizeof(*copy));

        if (used == capacity) {
            size_t next_capacity = capacity ? capacity * 2u : 8u;
            struct dirent **next = (struct dirent **) realloc(list, next_capacity * sizeof(*next));
            if (next == NULL) {
                free(copy);
                errno = ENOMEM;
                goto fail;
            }
            list = next;
            capacity = next_capacity;
        }

        list[used++] = copy;
    }

    oh_scandir_sort(list, used, compar);
    *entries = list;
    return (int) used;

fail:
    if (list) {
        for (size_t i = 0; i < used; ++i) {
            free(list[i]);
        }
        free(list);
    }
    *entries = NULL;
    return -1;
}

int scandir(const char *path, struct dirent ***entries, int (*filter)(const struct dirent *),
            int (*compar)(const struct dirent **, const struct dirent **)) {
    DIR *dir = opendir(path);
    int status;

    if (dir == NULL) {
        return -1;
    }

    status = oh_scandir_impl(dir, entries, filter, compar);
    closedir(dir);
    return status;
}

int scandirat(int dirfd_value, const char *path, struct dirent ***entries, int (*filter)(const struct dirent *),
              int (*compar)(const struct dirent **, const struct dirent **)) {
    int fd = openat(dirfd_value, path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    DIR *dir;
    int status;

    if (fd < 0) {
        return -1;
    }

    dir = fdopendir(fd);
    if (dir == NULL) {
        close(fd);
        return -1;
    }

    status = oh_scandir_impl(dir, entries, filter, compar);
    closedir(dir);
    return status;
}
