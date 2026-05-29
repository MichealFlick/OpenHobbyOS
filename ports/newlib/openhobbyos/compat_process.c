#include <errno.h>
#include <spawn.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "compat.h"

/* Force link of __on_exit_args for _REENT_SMALL newlib atexit cleanup */
extern char __on_exit_args;
static const void * const __ohos_force_on_exit __attribute__((used)) = &__on_exit_args;

struct __posix_spawn_file_actions {
    unsigned int count;
};

struct __posix_spawnattr {
    short flags;
    sigset_t sigmask;
    pid_t pgroup;
};

pid_t fork(void) {
    int result = oh_fork_raw();
    if (result < 0) {
        errno = -result;
        return -1;
    }
    return (pid_t) result;
}

int execve(const char *path, char *const argv[], char *const envp[]) {
    int result = oh_execve_raw(path, argv, envp);
    if (result < 0) {
        errno = -result;
        return -1;
    }
    return result;
}

int execvp(const char *file, char *const argv[]) {
    return execve(file, argv, environ);
}

int execvpe(const char *file, char *const argv[], char *const envp[]) {
    return execve(file, argv, envp);
}

int execl(const char *path, const char *arg, ...) {
    va_list ap;
    char *argv[32];
    int argc = 0;

    if (path == NULL || arg == NULL) {
        errno = EINVAL;
        return -1;
    }

    argv[argc++] = (char *) arg;

    va_start(ap, arg);
    for (;;) {
        char *next = va_arg(ap, char *);
        if (next == NULL || argc + 1 >= (int) (sizeof(argv) / sizeof(argv[0]))) {
            argv[argc] = NULL;
            break;
        }
        argv[argc++] = next;
    }
    va_end(ap);

    return execve(path, argv, environ);
}

pid_t waitpid(pid_t pid, int *status, int options) {
    int result = oh_waitpid_raw((int) pid, status, options);
    return (result < 0) ? (pid_t) oh_check_result(result) : (pid_t) result;
}

int kill(pid_t pid, int sig) {
    if (sig == 0 && (pid == 0 || pid == -1 || pid == getpid())) {
        return 0;
    }
    errno = ESRCH;
    return -1;
}

pid_t setsid(void) {
    return getpid();
}

int setegid(gid_t gid) {
    return setgid(gid);
}

int posix_spawn_file_actions_init(posix_spawn_file_actions_t *actions) {
    if (actions == NULL) {
        return EINVAL;
    }
    *actions = (posix_spawn_file_actions_t) calloc(1, sizeof(**actions));
    return *actions ? 0 : ENOMEM;
}

int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t *actions) {
    if (actions == NULL || *actions == NULL) {
        return 0;
    }
    free(*actions);
    *actions = NULL;
    return 0;
}

int posix_spawn_file_actions_addopen(posix_spawn_file_actions_t *actions, int fd, const char *path, int oflag, mode_t mode) {
    (void) fd;
    (void) path;
    (void) oflag;
    (void) mode;
    if (actions == NULL || *actions == NULL) {
        return EINVAL;
    }
    (*actions)->count++;
    return 0;
}

int posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t *actions, int fd, int newfd) {
    (void) fd;
    (void) newfd;
    if (actions == NULL || *actions == NULL) {
        return EINVAL;
    }
    (*actions)->count++;
    return 0;
}

int posix_spawn_file_actions_addclose(posix_spawn_file_actions_t *actions, int fd) {
    (void) fd;
    if (actions == NULL || *actions == NULL) {
        return EINVAL;
    }
    (*actions)->count++;
    return 0;
}

int posix_spawn_file_actions_addchdir(posix_spawn_file_actions_t *actions, const char *path) {
    (void) path;
    if (actions == NULL || *actions == NULL) {
        return EINVAL;
    }
    (*actions)->count++;
    return 0;
}

int posix_spawn_file_actions_addfchdir(posix_spawn_file_actions_t *actions, int fd) {
    (void) fd;
    if (actions == NULL || *actions == NULL) {
        return EINVAL;
    }
    (*actions)->count++;
    return 0;
}

int posix_spawnattr_init(posix_spawnattr_t *attr) {
    if (attr == NULL) {
        return EINVAL;
    }
    *attr = (posix_spawnattr_t) calloc(1, sizeof(**attr));
    return *attr ? 0 : ENOMEM;
}

int posix_spawnattr_destroy(posix_spawnattr_t *attr) {
    if (attr == NULL || *attr == NULL) {
        return 0;
    }
    free(*attr);
    *attr = NULL;
    return 0;
}

int posix_spawnattr_getflags(const posix_spawnattr_t *attr, short *flags) {
    if (attr == NULL || *attr == NULL || flags == NULL) {
        return EINVAL;
    }
    *flags = (*attr)->flags;
    return 0;
}

int posix_spawnattr_setflags(posix_spawnattr_t *attr, short flags) {
    if (attr == NULL || *attr == NULL) {
        return EINVAL;
    }
    (*attr)->flags = flags;
    return 0;
}

int posix_spawnattr_getsigmask(const posix_spawnattr_t *attr, sigset_t *sigmask) {
    if (attr == NULL || *attr == NULL || sigmask == NULL) {
        return EINVAL;
    }
    *sigmask = (*attr)->sigmask;
    return 0;
}

int posix_spawnattr_setsigmask(posix_spawnattr_t *attr, const sigset_t *sigmask) {
    if (attr == NULL || *attr == NULL || sigmask == NULL) {
        return EINVAL;
    }
    (*attr)->sigmask = *sigmask;
    return 0;
}

int posix_spawnattr_getpgroup(const posix_spawnattr_t *attr, pid_t *pgroup) {
    if (attr == NULL || *attr == NULL || pgroup == NULL) {
        return EINVAL;
    }
    *pgroup = (*attr)->pgroup;
    return 0;
}

int posix_spawnattr_setpgroup(posix_spawnattr_t *attr, pid_t pgroup) {
    if (attr == NULL || *attr == NULL) {
        return EINVAL;
    }
    (*attr)->pgroup = pgroup;
    return 0;
}

int posix_spawn(pid_t *pid, const char *path, const posix_spawn_file_actions_t *file_actions,
                const posix_spawnattr_t *attrp, char *const argv[], char *const envp[]) {
    (void) path;
    (void) file_actions;
    (void) attrp;
    int child_pid;

    child_pid = oh_spawn_raw(path, argv, envp);
    if (child_pid < 0) {
        return errno ? errno : ENOSYS;
    }

    if (pid != NULL) {
        *pid = (pid_t) child_pid;
    }

    return 0;
}

int posix_spawnp(pid_t *pid, const char *file, const posix_spawn_file_actions_t *file_actions,
                 const posix_spawnattr_t *attrp, char *const argv[], char *const envp[]) {
    return posix_spawn(pid, file, file_actions, attrp, argv, envp);
}
