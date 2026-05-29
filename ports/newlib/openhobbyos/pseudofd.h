#ifndef OPENHOBBYOS_PSEUDOFD_H
#define OPENHOBBYOS_PSEUDOFD_H

#include <stddef.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <sys/timerfd.h>

#ifdef st_atime
#undef st_atime
#endif
#ifdef st_mtime
#undef st_mtime
#endif
#ifdef st_ctime
#undef st_ctime
#endif

#include "abi/linux.h"

bool oh_pseudofd_is(int fd);
int oh_pseudofd_close(int fd);
ssize_t oh_pseudofd_read(int fd, void *buffer, size_t length);
int oh_pseudofd_fstat(int fd, struct stat *statbuf);

int oh_timerfd_create(int clockid, int flags);
int oh_timerfd_settime(int fd, int flags, const struct itimerspec *new_value, struct itimerspec *old_value);
int oh_timerfd_gettime(int fd, struct itimerspec *curr_value);

int oh_signalfd_create(int fd, const sigset_t *mask, int flags);

int oh_epoll_create1(int flags);
int oh_epoll_ctl(int epfd, int op, int fd, const struct epoll_event *event);
int oh_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);

#endif
