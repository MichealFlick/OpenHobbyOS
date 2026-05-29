#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "compat.h"

static const char oh_user_name[] = "root";
static const char oh_user_passwd[] = "x";
static const char oh_user_gecos[] = "OpenHobbyOS";
static const char oh_user_home[] = "/root";
static const char oh_user_shell[] = "/bin/gosh";
static const char oh_group_name[] = "root";

static struct passwd oh_passwd = {
    .pw_name = (char *) oh_user_name,
    .pw_passwd = (char *) oh_user_passwd,
    .pw_uid = 0,
    .pw_gid = 0,
    .pw_comment = (char *) oh_user_gecos,
    .pw_gecos = (char *) oh_user_gecos,
    .pw_dir = (char *) oh_user_home,
    .pw_shell = (char *) oh_user_shell,
};

static char *oh_group_members[] = { (char *) oh_user_name, NULL };
static struct group oh_group = {
    .gr_name = (char *) oh_group_name,
    .gr_passwd = (char *) oh_user_passwd,
    .gr_gid = 0,
    .gr_mem = oh_group_members,
};

static int oh_pack_string(char **slot, char **cursor, size_t *remaining, const char *text) {
    size_t length = strlen(text) + 1;

    if (*remaining < length) {
        return ERANGE;
    }

    memcpy(*cursor, text, length);
    *slot = *cursor;
    *cursor += length;
    *remaining -= length;
    return 0;
}

static int oh_fill_passwd(struct passwd *pwd, char *buffer, size_t size, struct passwd **result) {
    char *cursor = buffer;
    size_t remaining = size;
    int status;

    if (pwd == NULL || buffer == NULL || result == NULL) {
        return EINVAL;
    }

    status = oh_pack_string(&pwd->pw_name, &cursor, &remaining, oh_user_name);
    if (status == 0) {
        status = oh_pack_string(&pwd->pw_passwd, &cursor, &remaining, oh_user_passwd);
    }
    if (status == 0) {
        status = oh_pack_string(&pwd->pw_comment, &cursor, &remaining, oh_user_gecos);
    }
    if (status == 0) {
        status = oh_pack_string(&pwd->pw_gecos, &cursor, &remaining, oh_user_gecos);
    }
    if (status == 0) {
        status = oh_pack_string(&pwd->pw_dir, &cursor, &remaining, oh_user_home);
    }
    if (status == 0) {
        status = oh_pack_string(&pwd->pw_shell, &cursor, &remaining, oh_user_shell);
    }
    if (status != 0) {
        *result = NULL;
        return status;
    }

    pwd->pw_uid = 0;
    pwd->pw_gid = 0;
    *result = pwd;
    return 0;
}

static int oh_fill_group(struct group *grp, char *buffer, size_t size, struct group **result) {
    char *cursor = buffer;
    size_t remaining = size;
    int status;

    if (grp == NULL || buffer == NULL || result == NULL) {
        return EINVAL;
    }

    if (remaining < sizeof(char *) * 2u) {
        *result = NULL;
        return ERANGE;
    }

    grp->gr_mem = (char **) cursor;
    cursor += sizeof(char *) * 2u;
    remaining -= sizeof(char *) * 2u;

    status = oh_pack_string(&grp->gr_name, &cursor, &remaining, oh_group_name);
    if (status == 0) {
        status = oh_pack_string(&grp->gr_passwd, &cursor, &remaining, oh_user_passwd);
    }
    if (status == 0) {
        status = oh_pack_string(&grp->gr_mem[0], &cursor, &remaining, oh_user_name);
    }
    if (status != 0) {
        *result = NULL;
        return status;
    }

    grp->gr_mem[1] = NULL;
    grp->gr_gid = 0;
    *result = grp;
    return 0;
}

int uname(struct utsname *name) {
    struct linux_utsname native_name;
    int result;

    if (name == NULL) {
        errno = EFAULT;
        return -1;
    }

    result = oh_uname_raw(&native_name);
    if (result < 0) {
        errno = -result;
        return -1;
    }

    memcpy(name->sysname, native_name.sysname, sizeof(name->sysname));
    memcpy(name->nodename, native_name.nodename, sizeof(name->nodename));
    memcpy(name->release, native_name.release, sizeof(name->release));
    memcpy(name->version, native_name.version, sizeof(name->version));
    memcpy(name->machine, native_name.machine, sizeof(name->machine));
    return 0;
}

int gethostname(char *name, size_t len) {
    struct utsname uts;
    size_t needed;

    if (name == NULL || len == 0) {
        errno = EINVAL;
        return -1;
    }
    if (uname(&uts) != 0) {
        return -1;
    }

    needed = strlen(uts.nodename) + 1;
    if (needed > len) {
        errno = ENAMETOOLONG;
        return -1;
    }

    memcpy(name, uts.nodename, needed);
    return 0;
}

pid_t getpid(void) {
    return (pid_t) oh_getpid_raw();
}

pid_t getppid(void) {
    return 1;
}

uid_t getuid(void) {
    return (uid_t) oh_getuid_raw();
}

gid_t getgid(void) {
    return (gid_t) oh_getgid_raw();
}

uid_t geteuid(void) {
    return (uid_t) oh_geteuid_raw();
}

gid_t getegid(void) {
    return (gid_t) oh_getegid_raw();
}

int setuid(uid_t uid) {
    if (uid == 0) {
        return 0;
    }
    errno = EPERM;
    return -1;
}

int seteuid(uid_t uid) {
    return setuid(uid);
}

int setgid(gid_t gid) {
    if (gid == 0) {
        return 0;
    }
    errno = EPERM;
    return -1;
}

pid_t getpgrp(void) {
    return getpid();
}

int setpgid(pid_t pid, pid_t pgid) {
    pid_t self = getpid();

    if (pid != 0 && pid != self) {
        errno = ESRCH;
        return -1;
    }
    if (pgid != 0 && pgid != self) {
        errno = EPERM;
        return -1;
    }

    return 0;
}

char *getlogin(void) {
    return (char *) oh_user_name;
}

int getlogin_r(char *name, size_t namesize) {
    size_t needed = strlen(oh_user_name) + 1;

    if (name == NULL || namesize < needed) {
        return ERANGE;
    }

    memcpy(name, oh_user_name, needed);
    return 0;
}

struct passwd *getpwuid(uid_t uid) {
    if (uid != 0) {
        errno = ENOENT;
        return NULL;
    }
    return &oh_passwd;
}

struct passwd *getpwnam(const char *name) {
    if (name == NULL || strcmp(name, oh_user_name) != 0) {
        errno = ENOENT;
        return NULL;
    }
    return &oh_passwd;
}

int getpwuid_r(uid_t uid, struct passwd *pwd, char *buffer, size_t size, struct passwd **result) {
    if (uid != 0) {
        if (result) {
            *result = NULL;
        }
        return ENOENT;
    }
    return oh_fill_passwd(pwd, buffer, size, result);
}

int getpwnam_r(const char *name, struct passwd *pwd, char *buffer, size_t size, struct passwd **result) {
    if (name == NULL || strcmp(name, oh_user_name) != 0) {
        if (result) {
            *result = NULL;
        }
        return ENOENT;
    }
    return oh_fill_passwd(pwd, buffer, size, result);
}

struct group *getgrgid(gid_t gid) {
    if (gid != 0) {
        errno = ENOENT;
        return NULL;
    }
    return &oh_group;
}

struct group *getgrnam(const char *name) {
    if (name == NULL || strcmp(name, oh_group_name) != 0) {
        errno = ENOENT;
        return NULL;
    }
    return &oh_group;
}

int getgrgid_r(gid_t gid, struct group *grp, char *buffer, size_t size, struct group **result) {
    if (gid != 0) {
        if (result) {
            *result = NULL;
        }
        return ENOENT;
    }
    return oh_fill_group(grp, buffer, size, result);
}

int getgrnam_r(const char *name, struct group *grp, char *buffer, size_t size, struct group **result) {
    if (name == NULL || strcmp(name, oh_group_name) != 0) {
        if (result) {
            *result = NULL;
        }
        return ENOENT;
    }
    return oh_fill_group(grp, buffer, size, result);
}

long sysconf(int name) {
    switch (name) {
#ifdef _SC_ARG_MAX
        case _SC_ARG_MAX:
            return 4096;
#endif
#ifdef _SC_CLK_TCK
        case _SC_CLK_TCK:
            return 100;
#endif
#ifdef _SC_OPEN_MAX
        case _SC_OPEN_MAX:
            return 32;
#endif
#if defined(_SC_PAGESIZE)
        case _SC_PAGESIZE:
#endif
#if defined(_SC_PAGE_SIZE) && (!defined(_SC_PAGESIZE) || _SC_PAGE_SIZE != _SC_PAGESIZE)
        case _SC_PAGE_SIZE:
#endif
            return 4096;
#ifdef _SC_NPROCESSORS_CONF
        case _SC_NPROCESSORS_CONF:
            return 1;
#endif
#ifdef _SC_NPROCESSORS_ONLN
        case _SC_NPROCESSORS_ONLN:
            return 1;
#endif
#ifdef _SC_PHYS_PAGES
        case _SC_PHYS_PAGES:
            return 4096;
#endif
#ifdef _SC_AVPHYS_PAGES
        case _SC_AVPHYS_PAGES:
            return 2048;
#endif
#ifdef _SC_TTY_NAME_MAX
        case _SC_TTY_NAME_MAX:
            return 16;
#endif
#ifdef _SC_GETPW_R_SIZE_MAX
        case _SC_GETPW_R_SIZE_MAX:
            return 128;
#endif
#ifdef _SC_LOGIN_NAME_MAX
        case _SC_LOGIN_NAME_MAX:
            return 16;
#endif
#ifdef _SC_HOST_NAME_MAX
        case _SC_HOST_NAME_MAX:
            return 64;
#endif
        default:
            errno = EINVAL;
            return -1;
    }
}

int getpagesize(void) {
    return 4096;
}

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
    static sigset_t current_mask;

    if (oldset) {
        *oldset = current_mask;
    }

    if (set == NULL) {
        return 0;
    }

    switch (how) {
        case SIG_BLOCK:
            current_mask |= *set;
            return 0;
        case SIG_UNBLOCK:
            current_mask &= (sigset_t) ~(*set);
            return 0;
        case SIG_SETMASK:
            current_mask = *set;
            return 0;
        default:
            errno = EINVAL;
            return -1;
    }
}
