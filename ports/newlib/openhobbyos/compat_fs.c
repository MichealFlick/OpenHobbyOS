#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "compat.h"

static int oh_validate_fd(int fd) {
    struct stat st;

    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    if (fstat(fd, &st) == 0 || isatty(fd)) {
        return 0;
    }
    errno = EBADF;
    return -1;
}

static mode_t oh_process_umask = 022;

static int oh_try_openat(int dirfd, const char *path, int flags, int mode) {
    int result = oh_openat_raw(dirfd, path, flags, mode);

    if (result < 0 && (flags & O_DIRECTORY) == 0) {
        result = oh_openat_raw(dirfd, path, flags | O_DIRECTORY, mode);
    }
    return result;
}

static char *oh_path_push(char *output, size_t size, const char *component) {
    size_t used = strlen(output);
    size_t comp_len = strlen(component);

    if (used == 0) {
        strcpy(output, "/");
        used = 1;
    }

    if (strcmp(output, "/") != 0) {
        if (used + 1 >= size) {
            errno = ENAMETOOLONG;
            return NULL;
        }
        output[used++] = '/';
        output[used] = '\0';
    }

    if (used + comp_len >= size) {
        errno = ENAMETOOLONG;
        return NULL;
    }

    memcpy(output + used, component, comp_len + 1);
    return output;
}

static int oh_normalize_path(const char *path, char *output, size_t size) {
    char working[PATH_MAX];
    char *cursor;

    if (path == NULL || output == NULL || size == 0) {
        errno = EINVAL;
        return -1;
    }

    if (path[0] == '/') {
        if (strlen(path) >= sizeof(working)) {
            errno = ENAMETOOLONG;
            return -1;
        }
        strcpy(working, path);
    } else {
        if (getcwd(working, sizeof(working)) == NULL) {
            return -1;
        }
        if (strcmp(working, "/") != 0) {
            strncat(working, "/", sizeof(working) - strlen(working) - 1u);
        }
        strncat(working, path, sizeof(working) - strlen(working) - 1u);
    }

    strcpy(output, "/");
    cursor = working;
    while (*cursor) {
        char component[NAME_MAX + 1];
        size_t used = 0;

        while (*cursor == '/') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }

        while (*cursor && *cursor != '/') {
            if (used + 1 >= sizeof(component)) {
                errno = ENAMETOOLONG;
                return -1;
            }
            component[used++] = *cursor++;
        }
        component[used] = '\0';

        if (strcmp(component, ".") == 0) {
            continue;
        }
        if (strcmp(component, "..") == 0) {
            char *last = strrchr(output, '/');
            if (last && last != output) {
                *last = '\0';
            } else {
                strcpy(output, "/");
            }
            continue;
        }

        if (oh_path_push(output, size, component) == NULL) {
            return -1;
        }
    }

    return 0;
}

static int oh_maybe_follow_self_link(char *path, size_t size) {
    char target[PATH_MAX];
    int length = oh_readlink_raw(path, target, sizeof(target) - 1u);

    if (length < 0) {
        return 0;
    }

    target[length] = '\0';
    return oh_normalize_path(target, path, size);
}

int openat(int dirfd, const char *path, int flags, ...) {
    va_list ap;
    int mode = 0;
    int result;

    if ((flags & O_CREAT) != 0) {
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
    }

    result = oh_openat_raw(dirfd, path, flags, mode);
    return oh_check_result(result);
}

int faccessat(int dirfd, const char *path, int mode, int flags) {
    char full_path[PATH_MAX];

    if (flags != 0) {
        errno = EINVAL;
        return -1;
    }
    if (dirfd == AT_FDCWD || (path && path[0] == '/')) {
        return access(path, mode);
    }

    if (path == NULL || oh_validate_fd(dirfd) != 0) {
        return -1;
    }

    if (readlinkat(dirfd, ".", full_path, sizeof(full_path) - 1u) >= 0) {
        return access(path, mode);
    }

    errno = ENOSYS;
    return -1;
}

int readlinkat(int dirfd, const char *path, char *buffer, size_t size) {
    int result = oh_readlinkat_raw(dirfd, path, buffer, size);
    return oh_check_result(result);
}

int fstatat(int dirfd, const char *path, struct stat *statbuf, int flags) {
    int fd;
    int status;

    if (statbuf == NULL || path == NULL) {
        errno = EFAULT;
        return -1;
    }
    if (flags != 0
#ifdef AT_SYMLINK_NOFOLLOW
        && flags != AT_SYMLINK_NOFOLLOW
#endif
    ) {
        errno = EINVAL;
        return -1;
    }

    fd = oh_try_openat(dirfd, path, O_RDONLY | O_CLOEXEC, 0);
    if (fd < 0) {
        errno = -fd;
        return -1;
    }

    status = fstat(fd, statbuf);
    close(fd);
    return status;
}

int ioctl(int fd, unsigned long request, ...) {
    va_list ap;
    void *argp = NULL;
    int result;

    va_start(ap, request);
    argp = va_arg(ap, void *);
    va_end(ap);

    result = oh_ioctl_raw(fd, request, argp);
    return oh_check_result(result);
}

int fcntl(int fd, int cmd, ...) {
    va_list ap;
    long arg = 0;

    va_start(ap, cmd);
    arg = va_arg(ap, long);
    va_end(ap);

    if (oh_validate_fd(fd) != 0) {
        return -1;
    }

    switch (cmd) {
        case F_GETFD:
            return 0;
        case F_SETFD:
            return 0;
        case F_GETFL:
            return 0;
        case F_SETFL:
            (void) arg;
            return 0;
#ifdef F_DUPFD
        case F_DUPFD:
#endif
#ifdef F_DUPFD_CLOEXEC
        case F_DUPFD_CLOEXEC:
#endif
            errno = ENOSYS;
            return -1;
        default:
            errno = EINVAL;
            return -1;
    }
}

int chmod(const char *path, mode_t mode) {
    struct stat st;

    (void) mode;
    if (path == NULL) {
        errno = EFAULT;
        return -1;
    }
    if (stat(path, &st) != 0) {
        return -1;
    }
    return 0;
}

int fchmod(int fd, mode_t mode) {
    struct stat st;

    (void) mode;
    if (fstat(fd, &st) != 0) {
        return -1;
    }
    return 0;
}

int fchown(int fd, uid_t owner, gid_t group) {
    struct stat st;

    (void) owner;
    (void) group;
    if (fstat(fd, &st) != 0) {
        return -1;
    }
    return 0;
}

mode_t umask(mode_t mask) {
    mode_t previous = oh_process_umask;
    oh_process_umask = mask & 0777;
    return previous;
}

int fsync(int fd) {
    if (oh_validate_fd(fd) != 0) {
        return -1;
    }
    return 0;
}

int dup(int fd) {
    if (oh_validate_fd(fd) != 0) {
        return -1;
    }

    errno = ENOSYS;
    return -1;
}

int dup2(int oldfd, int newfd) {
    if (oh_validate_fd(oldfd) != 0) {
        return -1;
    }
    if (newfd < 0) {
        errno = EBADF;
        return -1;
    }
    if (oldfd == newfd) {
        return newfd;
    }

    errno = ENOSYS;
    return -1;
}

int pipe(int pipefd[2]) {
    (void) pipefd;
    errno = ENOSYS;
    return -1;
}

int pipe2(int pipefd[2], int flags) {
    (void) pipefd;
    (void) flags;
    errno = ENOSYS;
    return -1;
}

char *realpath(const char *path, char *resolved_path) {
    char normalized[PATH_MAX];
    char *target = resolved_path;
    struct stat st;

    if (oh_normalize_path(path, normalized, sizeof(normalized)) != 0) {
        return NULL;
    }

    if (oh_maybe_follow_self_link(normalized, sizeof(normalized)) != 0) {
        return NULL;
    }

    if (stat(normalized, &st) != 0) {
        return NULL;
    }

    if (target == NULL) {
        target = (char *) malloc(PATH_MAX);
        if (target == NULL) {
            errno = ENOMEM;
            return NULL;
        }
    }

    strcpy(target, normalized);
    return target;
}

char *canonicalize_file_name(const char *path) {
    return realpath(path, NULL);
}

char *get_current_dir_name(void) {
    char *buffer = (char *) malloc(PATH_MAX);

    if (buffer == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    if (getcwd(buffer, PATH_MAX) == NULL) {
        free(buffer);
        return NULL;
    }

    return buffer;
}
