#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "pseudofd.h"

#define OH_PSEUDOFD_BASE 0x40000000
#define OH_PSEUDOFD_MAX 64
#define OH_EPOLL_MAX_WATCHES 64

enum oh_pseudofd_kind {
    OH_PSEUDOFD_FREE = 0,
    OH_PSEUDOFD_TIMER,
    OH_PSEUDOFD_EPOLL,
    OH_PSEUDOFD_SIGNAL,
};

struct oh_timer_state {
    int clockid;
    int flags;
    struct itimerspec spec;
    struct timespec armed_at;
    bool armed;
};

struct oh_epoll_watch {
    bool used;
    int fd;
    struct epoll_event event;
};

struct oh_epoll_state {
    int flags;
    struct oh_epoll_watch watches[OH_EPOLL_MAX_WATCHES];
};

struct oh_signal_state {
    int flags;
    sigset_t mask;
};

struct oh_pseudofd_entry {
    enum oh_pseudofd_kind kind;
    union {
        struct oh_timer_state timer;
        struct oh_epoll_state epoll;
        struct oh_signal_state signal;
    } u;
};

static struct oh_pseudofd_entry g_entries[OH_PSEUDOFD_MAX];

static int oh_now(clockid_t clockid, struct timespec *ts) {
    if (clock_gettime(clockid, ts) != 0) {
        return -1;
    }
    return 0;
}

static int oh_timespec_cmp(const struct timespec *a, const struct timespec *b) {
    if (a->tv_sec != b->tv_sec) {
        return (a->tv_sec > b->tv_sec) - (a->tv_sec < b->tv_sec);
    }
    return (a->tv_nsec > b->tv_nsec) - (a->tv_nsec < b->tv_nsec);
}

static struct timespec oh_timespec_add(const struct timespec *a, const struct timespec *b) {
    struct timespec out;
    out.tv_sec = a->tv_sec + b->tv_sec;
    out.tv_nsec = a->tv_nsec + b->tv_nsec;
    if (out.tv_nsec >= 1000000000L) {
        out.tv_sec++;
        out.tv_nsec -= 1000000000L;
    }
    return out;
}

static struct timespec oh_timespec_sub(const struct timespec *a, const struct timespec *b) {
    struct timespec out;
    out.tv_sec = a->tv_sec - b->tv_sec;
    out.tv_nsec = a->tv_nsec - b->tv_nsec;
    if (out.tv_nsec < 0) {
        out.tv_sec--;
        out.tv_nsec += 1000000000L;
    }
    if (out.tv_sec < 0) {
        out.tv_sec = 0;
        out.tv_nsec = 0;
    }
    return out;
}

static int oh_entry_from_fd(int fd) {
    int index = fd - OH_PSEUDOFD_BASE;
    if (index < 0 || index >= OH_PSEUDOFD_MAX) {
        return -1;
    }
    return index;
}

static struct oh_pseudofd_entry *oh_entry_get(int fd, enum oh_pseudofd_kind expected) {
    int index = oh_entry_from_fd(fd);
    if (index < 0) {
        errno = EBADF;
        return NULL;
    }
    struct oh_pseudofd_entry *entry = &g_entries[index];
    if (entry->kind != expected) {
        errno = EBADF;
        return NULL;
    }
    return entry;
}

static int oh_entry_alloc(enum oh_pseudofd_kind kind) {
    for (int i = 0; i < OH_PSEUDOFD_MAX; ++i) {
        if (g_entries[i].kind == OH_PSEUDOFD_FREE) {
            memset(&g_entries[i], 0, sizeof(g_entries[i]));
            g_entries[i].kind = kind;
            return OH_PSEUDOFD_BASE + i;
        }
    }
    errno = EMFILE;
    return -1;
}

bool oh_pseudofd_is(int fd) {
    return oh_entry_from_fd(fd) >= 0;
}

static uint64_t oh_timerfd_expirations(struct oh_timer_state *timer, struct timespec *next_remaining) {
    struct timespec now;
    struct timespec first_deadline;
    uint64_t expirations = 0;

    if (!timer->armed) {
        if (next_remaining) {
            next_remaining->tv_sec = 0;
            next_remaining->tv_nsec = 0;
        }
        return 0;
    }
    if (oh_now((clockid_t)timer->clockid, &now) != 0) {
        return 0;
    }

    first_deadline = oh_timespec_add(&timer->armed_at, &timer->spec.it_value);
    if (oh_timespec_cmp(&now, &first_deadline) < 0) {
        if (next_remaining) {
            *next_remaining = oh_timespec_sub(&first_deadline, &now);
        }
        return 0;
    }

    expirations = 1;
    if (timer->spec.it_interval.tv_sec != 0 || timer->spec.it_interval.tv_nsec != 0) {
        struct timespec deadline = first_deadline;
        while (oh_timespec_cmp(&now, &deadline) >= 0) {
            expirations++;
            deadline = oh_timespec_add(&deadline, &timer->spec.it_interval);
        }
        if (next_remaining) {
            *next_remaining = oh_timespec_sub(&deadline, &now);
        }
        return expirations - 1;
    }

    if (next_remaining) {
        next_remaining->tv_sec = 0;
        next_remaining->tv_nsec = 0;
    }
    return expirations;
}

static int oh_fd_is_readable(int fd, uint32_t *out_events) {
    int bytes_available = 0;
    char dummy = 0;
    ssize_t peek_result;

    *out_events = 0;

    if (ioctl(fd, FIONREAD, &bytes_available) == 0) {
        if (bytes_available > 0) {
            *out_events |= EPOLLIN;
        }
        peek_result = recv(fd, &dummy, sizeof(dummy), MSG_PEEK | MSG_DONTWAIT);
        if (peek_result == 0) {
            *out_events |= EPOLLHUP;
        } else if (peek_result > 0) {
            *out_events |= EPOLLIN;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != ENOTCONN) {
            *out_events |= EPOLLERR;
        }
        return 0;
    }

    {
        struct stat st;
        if (fstat(fd, &st) == 0 || isatty(fd)) {
            *out_events |= EPOLLIN | EPOLLOUT;
            return 0;
        }
    }

    return -1;
}

int oh_pseudofd_close(int fd) {
    int index = oh_entry_from_fd(fd);
    if (index < 0 || g_entries[index].kind == OH_PSEUDOFD_FREE) {
        errno = EBADF;
        return -1;
    }
    memset(&g_entries[index], 0, sizeof(g_entries[index]));
    return 0;
}

ssize_t oh_pseudofd_read(int fd, void *buffer, size_t length) {
    struct oh_pseudofd_entry *entry = oh_entry_get(fd, OH_PSEUDOFD_TIMER);
    uint64_t expirations;

    if (!entry) {
        return -1;
    }
    if (buffer == NULL || length < sizeof(uint64_t)) {
        errno = EINVAL;
        return -1;
    }

    expirations = oh_timerfd_expirations(&entry->u.timer, NULL);
    if (expirations == 0) {
        errno = EAGAIN;
        return -1;
    }

    memcpy(buffer, &expirations, sizeof(expirations));
    if (entry->u.timer.spec.it_interval.tv_sec == 0 && entry->u.timer.spec.it_interval.tv_nsec == 0) {
        entry->u.timer.armed = false;
    } else if (oh_now((clockid_t)entry->u.timer.clockid, &entry->u.timer.armed_at) != 0) {
        entry->u.timer.armed = false;
    }

    return (ssize_t)sizeof(expirations);
}

int oh_pseudofd_fstat(int fd, struct stat *statbuf) {
    int index = oh_entry_from_fd(fd);
    if (index < 0 || g_entries[index].kind == OH_PSEUDOFD_FREE || statbuf == NULL) {
        errno = EBADF;
        return -1;
    }

    memset(statbuf, 0, sizeof(*statbuf));
    statbuf->st_mode = S_IFCHR;
    statbuf->st_nlink = 1;
    statbuf->st_blksize = 8;
    return 0;
}

int oh_timerfd_create(int clockid, int flags) {
    int fd = oh_entry_alloc(OH_PSEUDOFD_TIMER);
    if (fd < 0) {
        return -1;
    }

    struct oh_pseudofd_entry *entry = oh_entry_get(fd, OH_PSEUDOFD_TIMER);
    entry->u.timer.clockid = clockid;
    entry->u.timer.flags = flags;
    entry->u.timer.armed = false;
    memset(&entry->u.timer.spec, 0, sizeof(entry->u.timer.spec));
    return fd;
}

int oh_timerfd_settime(int fd, int flags, const struct itimerspec *new_value, struct itimerspec *old_value) {
    struct oh_pseudofd_entry *entry = oh_entry_get(fd, OH_PSEUDOFD_TIMER);

    (void)flags;
    if (!entry || new_value == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (old_value != NULL) {
        oh_timerfd_gettime(fd, old_value);
    }

    entry->u.timer.spec = *new_value;
    entry->u.timer.armed = (new_value->it_value.tv_sec != 0 || new_value->it_value.tv_nsec != 0);
    if (entry->u.timer.armed && oh_now((clockid_t)entry->u.timer.clockid, &entry->u.timer.armed_at) != 0) {
        entry->u.timer.armed = false;
        return -1;
    }
    return 0;
}

int oh_timerfd_gettime(int fd, struct itimerspec *curr_value) {
    struct oh_pseudofd_entry *entry = oh_entry_get(fd, OH_PSEUDOFD_TIMER);
    struct timespec remaining;

    if (!entry || curr_value == NULL) {
        errno = EINVAL;
        return -1;
    }

    memset(curr_value, 0, sizeof(*curr_value));
    curr_value->it_interval = entry->u.timer.spec.it_interval;
    oh_timerfd_expirations(&entry->u.timer, &remaining);
    if (entry->u.timer.armed) {
        curr_value->it_value = remaining;
    }
    return 0;
}

int oh_signalfd_create(int fd, const sigset_t *mask, int flags) {
    int new_fd;
    struct oh_pseudofd_entry *entry;

    (void)fd;
    if (mask == NULL) {
        errno = EINVAL;
        return -1;
    }

    new_fd = oh_entry_alloc(OH_PSEUDOFD_SIGNAL);
    if (new_fd < 0) {
        return -1;
    }

    entry = oh_entry_get(new_fd, OH_PSEUDOFD_SIGNAL);
    entry->u.signal.flags = flags;
    entry->u.signal.mask = *mask;
    return new_fd;
}

int oh_epoll_create1(int flags) {
    int fd = oh_entry_alloc(OH_PSEUDOFD_EPOLL);
    struct oh_pseudofd_entry *entry;

    if (fd < 0) {
        return -1;
    }
    entry = oh_entry_get(fd, OH_PSEUDOFD_EPOLL);
    entry->u.epoll.flags = flags;
    return fd;
}

int oh_epoll_ctl(int epfd, int op, int fd, const struct epoll_event *event) {
    struct oh_pseudofd_entry *entry = oh_entry_get(epfd, OH_PSEUDOFD_EPOLL);
    struct oh_epoll_state *epoll;
    int free_slot = -1;

    if (!entry) {
        return -1;
    }
    epoll = &entry->u.epoll;

    for (int i = 0; i < OH_EPOLL_MAX_WATCHES; ++i) {
        if (!epoll->watches[i].used && free_slot < 0) {
            free_slot = i;
        }
        if (epoll->watches[i].used && epoll->watches[i].fd == fd) {
            if (op == EPOLL_CTL_DEL) {
                memset(&epoll->watches[i], 0, sizeof(epoll->watches[i]));
                return 0;
            }
            if (event == NULL) {
                errno = EINVAL;
                return -1;
            }
            epoll->watches[i].event = *event;
            return 0;
        }
    }

    if (op == EPOLL_CTL_DEL) {
        errno = ENOENT;
        return -1;
    }
    if (event == NULL || free_slot < 0) {
        errno = event == NULL ? EINVAL : ENOSPC;
        return -1;
    }

    epoll->watches[free_slot].used = true;
    epoll->watches[free_slot].fd = fd;
    epoll->watches[free_slot].event = *event;
    return 0;
}

int oh_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout) {
    struct oh_pseudofd_entry *entry = oh_entry_get(epfd, OH_PSEUDOFD_EPOLL);
    struct timespec deadline = {0};
    bool wait_forever = timeout < 0;

    if (!entry || events == NULL || maxevents <= 0) {
        errno = EINVAL;
        return -1;
    }

    if (!wait_forever) {
        if (oh_now(CLOCK_MONOTONIC, &deadline) != 0) {
            return -1;
        }
        struct timespec delta = {
            .tv_sec = timeout / 1000,
            .tv_nsec = (long)((timeout % 1000) * 1000000L),
        };
        deadline = oh_timespec_add(&deadline, &delta);
    }

    for (;;) {
        int ready = 0;

        for (int i = 0; i < OH_EPOLL_MAX_WATCHES && ready < maxevents; ++i) {
            uint32_t available = 0;
            struct oh_epoll_watch *watch = &entry->u.epoll.watches[i];
            if (!watch->used) {
                continue;
            }

            if (oh_pseudofd_is(watch->fd)) {
                int index = oh_entry_from_fd(watch->fd);
                if (index >= 0 && g_entries[index].kind == OH_PSEUDOFD_TIMER) {
                    if (oh_timerfd_expirations(&g_entries[index].u.timer, NULL) > 0) {
                        available |= EPOLLIN;
                    }
                }
            } else if (oh_fd_is_readable(watch->fd, &available) != 0) {
                available |= EPOLLERR;
            }

            if ((watch->event.events & EPOLLOUT) != 0 && !oh_pseudofd_is(watch->fd)) {
                available |= EPOLLOUT;
            }

            available &= (watch->event.events | EPOLLERR | EPOLLHUP);
            if (available == 0) {
                continue;
            }

            events[ready] = watch->event;
            events[ready].events = available;
            ready++;
        }

        if (ready > 0) {
            return ready;
        }
        if (!wait_forever) {
            struct timespec now;
            if (oh_now(CLOCK_MONOTONIC, &now) != 0) {
                return -1;
            }
            if (oh_timespec_cmp(&now, &deadline) >= 0) {
                return 0;
            }
        }

        {
            struct timespec sleep_time = {.tv_sec = 0, .tv_nsec = 1000000L};
            nanosleep(&sleep_time, NULL);
        }
    }
}
