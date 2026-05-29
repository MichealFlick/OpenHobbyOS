#ifndef OHOS_SOCKET_H
#define OHOS_SOCKET_H

#include "abi/linux.h"
#include "types.h"
#include "vfs.h"
#include "idt.h"

#define SOCKET_MAX_OBJECTS 64
#define SOCKET_BACKLOG 8
#define SOCKET_BUFFER_SIZE 65536
#define SOCKET_CONTROL_MAX 64

typedef enum {
    SOCKET_STATE_FREE = 0,
    SOCKET_STATE_CREATED,
    SOCKET_STATE_BOUND,
    SOCKET_STATE_LISTENING,
    SOCKET_STATE_CONNECTED,
    SOCKET_STATE_CLOSED,
} socket_state_t;

typedef enum {
    SOCKET_WAIT_NONE = 0,
    SOCKET_WAIT_ACCEPT,
    SOCKET_WAIT_READ,
} socket_wait_type_t;

typedef struct socket_endpoint socket_endpoint_t;

struct socket_endpoint {
    bool used;
    int id;
    int refcount;
    int domain;
    int type;
    int protocol;
    socket_state_t state;
    bool bound;
    bool listening;
    bool abstract;
    char path[VFS_PATH_MAX];
    int backlog;
    int pending[SOCKET_BACKLOG];
    int pending_head;
    int pending_tail;

    int accept_waiter;
    int accept_reserved_fd;
    u32 accept_addr;
    u32 accept_addrlen;

    int read_waiter;
    u32 read_buffer;
    u32 read_length;
    int read_flags;

    bool peer_closed;
    socket_endpoint_t *peer;
    u32 recv_head;
    u32 recv_tail;
    u8 recv_buffer[SOCKET_BUFFER_SIZE];
    const vfs_node_t *control_nodes[SOCKET_CONTROL_MAX];
    u32 control_head;
    u32 control_tail;
    bool waiting_for_read;
};

void socket_init(void);
socket_endpoint_t *socket_create(int domain, int type, int protocol);
int socket_bind(socket_endpoint_t *socket, const struct sockaddr *addr, socklen_t addrlen);
int socket_listen(socket_endpoint_t *socket, int backlog);
int socket_connect(socket_endpoint_t *socket, const struct sockaddr *addr, socklen_t addrlen);
int socket_accept_pending(socket_endpoint_t *listener, socket_endpoint_t **accepted);
ssize_t socket_recv(socket_endpoint_t *socket, void *buffer, size_t length, int flags);
ssize_t socket_send(socket_endpoint_t *socket, const void *buffer, size_t length, int flags);
int socket_shutdown(socket_endpoint_t *socket, int how);
int socket_getsockname(socket_endpoint_t *socket, struct sockaddr *addr, socklen_t *addrlen);
int socket_getpeername(socket_endpoint_t *socket, struct sockaddr *addr, socklen_t *addrlen);
int socket_setsockopt(socket_endpoint_t *socket, int level, int optname, const void *optval, socklen_t optlen);
int socket_getsockopt(socket_endpoint_t *socket, int level, int optname, void *optval, socklen_t *optlen);
bool socket_unlink_path(const char *path);
int socket_stat_path(const char *path, struct linux_stat64 *statbuf);
int socket_pending_readable(const socket_endpoint_t *socket);
int socket_enqueue_passed_node(socket_endpoint_t *socket, const vfs_node_t *node);
int socket_dequeue_passed_node(socket_endpoint_t *socket, const vfs_node_t **node);
void socket_release(socket_endpoint_t *socket);
int socket_is_socket_fd(int fd);

#endif
