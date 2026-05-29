#ifndef OHOS_PIPE_H
#define OHOS_PIPE_H

#include "abi/linux.h"
#include "types.h"

#define PIPE_MAX_OBJECTS 64
#define PIPE_BUFFER_SIZE 65536

typedef struct pipe_endpoint pipe_endpoint_t;

struct pipe_endpoint {
    bool used;
    int id;
    int refcount;
    bool read_open;
    bool write_open;
    bool peer_closed;
    int read_waiter;
    bool waiting_for_read;
    u32 recv_head;
    u32 recv_tail;
    u8 recv_buffer[PIPE_BUFFER_SIZE];
};

void pipe_init(void);
pipe_endpoint_t *pipe_create(void);
void pipe_release(pipe_endpoint_t *pipe);
ssize_t pipe_read(pipe_endpoint_t *pipe, void *buffer, size_t length, int flags);
ssize_t pipe_write(pipe_endpoint_t *pipe, const void *buffer, size_t length, int flags);
int pipe_close_read(pipe_endpoint_t *pipe);
int pipe_close_write(pipe_endpoint_t *pipe);
bool pipe_is_pipe_fd(int fd);
int pipe_pending_readable(pipe_endpoint_t *pipe);

#endif
