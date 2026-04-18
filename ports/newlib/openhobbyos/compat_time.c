#include <errno.h>
#include <time.h>
#include <unistd.h>

#include "compat.h"

int gettimeofday(struct timeval *tv, void *tz) {
    struct linux_timeval native_tv;
    int result;

    if (tv == NULL) {
        return 0;
    }

    result = oh_gettimeofday_raw(&native_tv, tz);
    if (result < 0) {
        errno = -result;
        return -1;
    }

    tv->tv_sec = native_tv.tv_sec;
    tv->tv_usec = native_tv.tv_usec;
    return 0;
}

int clock_gettime(clockid_t clock_id, struct timespec *tp) {
    struct linux_timespec native_tp;
    int native_clock = LINUX_CLOCK_REALTIME;
    int result;

    if (tp == NULL) {
        errno = EFAULT;
        return -1;
    }

    if (clock_id == CLOCK_MONOTONIC) {
        native_clock = LINUX_CLOCK_MONOTONIC;
    } else if (clock_id != CLOCK_REALTIME
#ifdef CLOCK_BOOTTIME
               && clock_id != CLOCK_BOOTTIME
#endif
    ) {
        errno = EINVAL;
        return -1;
    }

    result = oh_clock_gettime_raw(native_clock, &native_tp);
    if (result < 0) {
        errno = -result;
        return -1;
    }

    tp->tv_sec = native_tp.tv_sec;
    tp->tv_nsec = native_tp.tv_nsec;
    return 0;
}

int clock_getres(clockid_t clock_id, struct timespec *res) {
    (void) clock_id;

    if (res == NULL) {
        errno = EFAULT;
        return -1;
    }

    res->tv_sec = 0;
    res->tv_nsec = 1000000;
    return 0;
}

int nanosleep(const struct timespec *rqtp, struct timespec *rmtp) {
    struct linux_timespec req;
    struct linux_timespec rem;
    int result;

    if (rqtp == NULL) {
        errno = EFAULT;
        return -1;
    }
    if (rqtp->tv_sec < 0 || rqtp->tv_nsec < 0 || rqtp->tv_nsec >= 1000000000L) {
        errno = EINVAL;
        return -1;
    }

    req.tv_sec = (i32) rqtp->tv_sec;
    req.tv_nsec = (i32) rqtp->tv_nsec;
    result = oh_nanosleep_raw(&req, rmtp ? &rem : NULL);
    if (result < 0) {
        errno = -result;
        return -1;
    }

    if (rmtp) {
        rmtp->tv_sec = rem.tv_sec;
        rmtp->tv_nsec = rem.tv_nsec;
    }
    return 0;
}

int clock_nanosleep(clockid_t clock_id, int flags, const struct timespec *rqtp, struct timespec *rmtp) {
    if (flags != 0) {
        return EINVAL;
    }
    if (clock_id != CLOCK_REALTIME && clock_id != CLOCK_MONOTONIC
#ifdef CLOCK_BOOTTIME
        && clock_id != CLOCK_BOOTTIME
#endif
    ) {
        return EINVAL;
    }
    if (nanosleep(rqtp, rmtp) != 0) {
        return errno;
    }
    return 0;
}

time_t time(time_t *timer) {
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0) {
        return (time_t) -1;
    }
    if (timer) {
        *timer = tv.tv_sec;
    }
    return tv.tv_sec;
}

int usleep(useconds_t usec) {
    struct timespec req;

    req.tv_sec = (time_t) (usec / 1000000u);
    req.tv_nsec = (long) ((usec % 1000000u) * 1000u);
    return nanosleep(&req, NULL);
}

unsigned int sleep(unsigned int seconds) {
    struct timespec req;

    req.tv_sec = (time_t) seconds;
    req.tv_nsec = 0;

    if (nanosleep(&req, &req) == 0) {
        return 0;
    }

    return (unsigned int) req.tv_sec;
}
