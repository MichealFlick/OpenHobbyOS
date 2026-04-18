#include <errno.h>
#include <sys/resource.h>

int getrlimit(int resource, struct rlimit *rlim) {
    if (rlim == NULL) {
        errno = EINVAL;
        return -1;
    }

    switch (resource) {
        case RLIMIT_CORE:
        case RLIMIT_CPU:
        case RLIMIT_DATA:
        case RLIMIT_FSIZE:
        case RLIMIT_STACK:
            rlim->rlim_cur = RLIM_INFINITY;
            rlim->rlim_max = RLIM_INFINITY;
            return 0;
        default:
            errno = EINVAL;
            return -1;
    }
}

int setrlimit(int resource, const struct rlimit *rlim) {
    if (rlim == NULL) {
        errno = EINVAL;
        return -1;
    }

    switch (resource) {
        case RLIMIT_CORE:
        case RLIMIT_CPU:
        case RLIMIT_DATA:
        case RLIMIT_FSIZE:
        case RLIMIT_STACK:
            return 0;
        default:
            errno = EINVAL;
            return -1;
    }
}
