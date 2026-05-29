#include "pipe.h"
#include "task.h"
#include "string.h"
#include <stddef.h>

static pipe_endpoint_t pipe_endpoints[PIPE_MAX_OBJECTS];

static void pipe_buffer_reset(pipe_endpoint_t *pipe) {
    pipe->recv_head = 0;
    pipe->recv_tail = 0;
}

static u32 pipe_buffer_used(const pipe_endpoint_t *pipe) {
    if (pipe->recv_tail >= pipe->recv_head) {
        return pipe->recv_tail - pipe->recv_head;
    }
    return (PIPE_BUFFER_SIZE - pipe->recv_head) + pipe->recv_tail;
}

static u32 pipe_buffer_available(const pipe_endpoint_t *pipe) {
    return PIPE_BUFFER_SIZE - pipe_buffer_used(pipe) - 1;
}

static ssize_t pipe_buffer_write(pipe_endpoint_t *pipe, const void *buffer, size_t length) {
    u32 available = pipe_buffer_available(pipe);
    if (available == 0) {
        return 0;
    }
    u32 to_write = (u32)length;
    if (to_write > available) {
        to_write = available;
    }
    const u8 *src = (const u8 *)buffer;
    u32 written = 0;

    while (written < to_write) {
        u32 chunk = (u32)PIPE_BUFFER_SIZE - pipe->recv_tail;
        if (chunk > to_write - written) {
            chunk = to_write - written;
        }
        memcpy(&pipe->recv_buffer[pipe->recv_tail], src + written, chunk);
        pipe->recv_tail = (pipe->recv_tail + chunk) % PIPE_BUFFER_SIZE;
        written += chunk;
    }

    return (ssize_t)written;
}

static ssize_t pipe_buffer_read(pipe_endpoint_t *pipe, void *buffer, size_t length) {
    u32 available = pipe_buffer_used(pipe);
    if (available == 0) {
        return 0;
    }
    u32 to_read = (u32)length;
    if (to_read > available) {
        to_read = available;
    }
    u8 *dest = (u8 *)buffer;
    u32 read = 0;

    while (read < to_read) {
        u32 chunk = (u32)PIPE_BUFFER_SIZE - pipe->recv_head;
        if (chunk > to_read - read) {
            chunk = to_read - read;
        }
        memcpy(dest + read, &pipe->recv_buffer[pipe->recv_head], chunk);
        pipe->recv_head = (pipe->recv_head + chunk) % PIPE_BUFFER_SIZE;
        read += chunk;
    }

    return (ssize_t)read;
}

static int pipe_next_id(void) {
    static int next_id = 1;
    return next_id++;
}

static pipe_endpoint_t *pipe_alloc(void) {
    for (int i = 0; i < PIPE_MAX_OBJECTS; ++i) {
        if (!pipe_endpoints[i].used) {
            pipe_endpoint_t *pipe = &pipe_endpoints[i];
            memset(pipe, 0, sizeof(*pipe));
            pipe->used = true;
            pipe->id = pipe_next_id();
            pipe->refcount = 1;
            pipe->read_open = true;
            pipe->write_open = true;
            pipe->peer_closed = false;
            pipe->read_waiter = -1;
            pipe->waiting_for_read = false;
            pipe_buffer_reset(pipe);
            return pipe;
        }
    }
    return NULL;
}

void pipe_init(void) {
    memset(pipe_endpoints, 0, sizeof(pipe_endpoints));
}

pipe_endpoint_t *pipe_create(void) {
    return pipe_alloc();
}

void pipe_release(pipe_endpoint_t *pipe) {
    if (!pipe || !pipe->used) {
        return;
    }
    if (--pipe->refcount > 0) {
        return;
    }
    pipe->used = false;
    pipe->read_open = false;
    pipe->write_open = false;
    pipe->peer_closed = true;
}

ssize_t pipe_read(pipe_endpoint_t *pipe, void *buffer, size_t length, int flags) {
    (void)flags;

    if (!pipe || !pipe->used) {
        return -EBADF;
    }
    if (!pipe->read_open) {
        return -EBADF;
    }
    if (!buffer) {
        return -EFAULT;
    }

    ssize_t result = pipe_buffer_read(pipe, buffer, length);
    if (result > 0) {
        return result;
    }

    /* No data available */
    if (!pipe->write_open || pipe->peer_closed) {
        /* Write end is closed, return EOF (0) */
        return 0;
    }

    /* Would block - for now return 0 (EAGAIN behavior) */
    return 0;
}

ssize_t pipe_write(pipe_endpoint_t *pipe, const void *buffer, size_t length, int flags) {
    (void)flags;

    if (!pipe || !pipe->used) {
        return -EBADF;
    }
    if (!pipe->write_open) {
        return -EBADF;
    }
    if (!buffer) {
        return -EFAULT;
    }
    if (!pipe->read_open) {
        /* Read end is closed - SIGPIPE/EPIPE */
        return -EPIPE;
    }

    return pipe_buffer_write(pipe, buffer, length);
}

int pipe_close_read(pipe_endpoint_t *pipe) {
    if (!pipe || !pipe->used) {
        return -EBADF;
    }
    if (!pipe->read_open) {
        return -EBADF;
    }
    pipe->read_open = false;
    /* Wake up any waiters */
    if (pipe->read_waiter >= 0 && pipe->waiting_for_read) {
        task_wake_slot(pipe->read_waiter, 0);
        pipe->read_waiter = -1;
        pipe->waiting_for_read = false;
    }
    return 0;
}

int pipe_close_write(pipe_endpoint_t *pipe) {
    if (!pipe || !pipe->used) {
        return -EBADF;
    }
    if (!pipe->write_open) {
        return -EBADF;
    }
    pipe->write_open = false;
    pipe->peer_closed = true;
    /* Wake up readers so they see EOF */
    if (pipe->read_waiter >= 0 && pipe->waiting_for_read) {
        task_wake_slot(pipe->read_waiter, 0);
        pipe->read_waiter = -1;
        pipe->waiting_for_read = false;
    }
    return 0;
}

bool pipe_is_pipe_fd(int fd) {
    task_fd_t *slot = task_fd_slot(fd);
    return slot && slot->used && (slot->kind == TASK_FD_PIPE_READ || slot->kind == TASK_FD_PIPE_WRITE);
}

int pipe_pending_readable(pipe_endpoint_t *pipe) {
    if (!pipe || !pipe->read_open) {
        return 0;
    }
    return (int)pipe_buffer_used(pipe);
}
