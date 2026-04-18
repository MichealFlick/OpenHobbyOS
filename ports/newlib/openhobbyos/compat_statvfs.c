#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

static unsigned long oh_align_up(unsigned long value, unsigned long alignment) {
    return (value + alignment - 1u) / alignment;
}

static int oh_sum_tree(const char *path, unsigned long long *bytes, unsigned long long *files) {
    struct stat st;

    if (stat(path, &st) != 0) {
        return -1;
    }

    if (S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(path);
        struct dirent *entry;
        char child[PATH_MAX];

        if (dir == NULL) {
            return -1;
        }

        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            if (strcmp(path, "/") == 0) {
                snprintf(child, sizeof(child), "/%s", entry->d_name);
            } else {
                snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
            }

            if (oh_sum_tree(child, bytes, files) != 0) {
                closedir(dir);
                return -1;
            }
        }

        closedir(dir);
        (*files)++;
        return 0;
    }

    if (st.st_size > 0) {
        *bytes += (unsigned long long) st.st_size;
    }
    (*files)++;
    return 0;
}

static int oh_fill_statvfs_path(const char *path, struct statvfs *statvfsbuf) {
    unsigned long long bytes = 0;
    unsigned long long files = 0;

    if (statvfsbuf == NULL) {
        errno = EFAULT;
        return -1;
    }

    if (oh_sum_tree(path, &bytes, &files) != 0) {
        return -1;
    }

    memset(statvfsbuf, 0, sizeof(*statvfsbuf));
    statvfsbuf->f_bsize = 1024u;
    statvfsbuf->f_frsize = 1024u;
    statvfsbuf->f_blocks = (fsblkcnt_t) oh_align_up((unsigned long) bytes, statvfsbuf->f_frsize);
    statvfsbuf->f_bfree = 0;
    statvfsbuf->f_bavail = 0;
    statvfsbuf->f_files = (fsfilcnt_t) files;
    statvfsbuf->f_ffree = 0;
    statvfsbuf->f_favail = 0;
    statvfsbuf->f_flag = ST_RDONLY;
    statvfsbuf->f_namemax = 255u;
    return 0;
}

int statvfs(const char *path, struct statvfs *statvfsbuf) {
    return oh_fill_statvfs_path(path, statvfsbuf);
}

int fstatvfs(int fd, struct statvfs *statvfsbuf) {
    struct stat st;

    if (statvfsbuf == NULL) {
        errno = EFAULT;
        return -1;
    }

    if (fstat(fd, &st) != 0) {
        return -1;
    }

    memset(statvfsbuf, 0, sizeof(*statvfsbuf));
    statvfsbuf->f_bsize = 1024u;
    statvfsbuf->f_frsize = 1024u;
    statvfsbuf->f_blocks = (fsblkcnt_t) oh_align_up((unsigned long) (st.st_size < 0 ? 0 : st.st_size), statvfsbuf->f_frsize);
    statvfsbuf->f_bfree = 0;
    statvfsbuf->f_bavail = 0;
    statvfsbuf->f_files = 1;
    statvfsbuf->f_ffree = 0;
    statvfsbuf->f_favail = 0;
    statvfsbuf->f_flag = ST_RDONLY;
    statvfsbuf->f_namemax = 255u;
    return 0;
}

int statvfs64(const char *path, struct statvfs64 *statvfsbuf) {
    struct statvfs native_buf;

    if (statvfs(path, &native_buf) != 0) {
        return -1;
    }

    memcpy(statvfsbuf, &native_buf, sizeof(native_buf));
    return 0;
}

int fstatvfs64(int fd, struct statvfs64 *statvfsbuf) {
    struct statvfs native_buf;

    if (fstatvfs(fd, &native_buf) != 0) {
        return -1;
    }

    memcpy(statvfsbuf, &native_buf, sizeof(native_buf));
    return 0;
}
