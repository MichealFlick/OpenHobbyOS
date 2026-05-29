#include "socket.h"
#include "console.h"
#include "task.h"
#include "memory.h"
#include "string.h"
#include <stddef.h>

static socket_endpoint_t socket_endpoints[SOCKET_MAX_OBJECTS];

static void socket_try_complete_accept(socket_endpoint_t *listener) {
    if (!listener || listener->accept_waiter < 0) {
        return;
    }

    socket_endpoint_t *accepted = NULL;
    int pending = socket_accept_pending(listener, &accepted);
    if (pending < 0 || !accepted) {
        return;
    }

    int slot = listener->accept_waiter;
    int new_fd = task_slot_install_socket(slot, accepted);
    if (new_fd < 0) {
        socket_release(accepted);
        listener->accept_waiter = -1;
        listener->accept_addr = 0;
        listener->accept_addrlen = 0;
        task_wake_slot(slot, new_fd);
        return;
    }

    if (listener->accept_addr != 0 && listener->accept_addrlen != 0) {
        struct sockaddr_un peer;
        socklen_t len = sizeof(sa_family_t);
        memset(&peer, 0, sizeof(peer));
        peer.sun_family = AF_UNIX;

        (void)task_write_slot_user(slot, listener->accept_addr, &peer, len);
        (void)task_write_slot_user(slot, listener->accept_addrlen, &len, sizeof(len));
    }

    listener->accept_waiter = -1;
    listener->accept_addr = 0;
    listener->accept_addrlen = 0;
    task_wake_slot(slot, new_fd);
}

static int socket_next_id(void) {
    static int next_id = 1;
    return next_id++;
}

static socket_endpoint_t *socket_alloc(void) {
    for (int i = 0; i < SOCKET_MAX_OBJECTS; ++i) {
        if (!socket_endpoints[i].used) {
            socket_endpoint_t *sock = &socket_endpoints[i];
            memset(sock, 0, sizeof(*sock));
            sock->used = true;
            sock->id = socket_next_id();
            sock->refcount = 1;
            sock->state = SOCKET_STATE_CREATED;
            sock->accept_waiter = -1;
            sock->read_waiter = -1;
            return sock;
        }
    }
    return NULL;
}

static void socket_buffer_reset(socket_endpoint_t *socket) {
    socket->recv_head = 0;
    socket->recv_tail = 0;
}

static u32 socket_buffer_used(const socket_endpoint_t *socket) {
    if (socket->recv_tail >= socket->recv_head) {
        return socket->recv_tail - socket->recv_head;
    }
    return (SOCKET_BUFFER_SIZE - socket->recv_head) + socket->recv_tail;
}

static u32 socket_buffer_available(const socket_endpoint_t *socket) {
    return SOCKET_BUFFER_SIZE - socket_buffer_used(socket) - 1;
}

static u32 socket_control_count(const socket_endpoint_t *socket) {
    if (socket->control_tail >= socket->control_head) {
        return socket->control_tail - socket->control_head;
    }
    return SOCKET_CONTROL_MAX - socket->control_head + socket->control_tail;
}

static bool socket_control_full(const socket_endpoint_t *socket) {
    return socket_control_count(socket) >= SOCKET_CONTROL_MAX - 1;
}

static ssize_t socket_buffer_write(socket_endpoint_t *socket, const void *buffer, size_t length) {
    u32 available = socket_buffer_available(socket);
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
        u32 chunk = (u32)SOCKET_BUFFER_SIZE - socket->recv_tail;
        if (chunk > to_write - written) {
            chunk = to_write - written;
        }
        memcpy(&socket->recv_buffer[socket->recv_tail], src + written, chunk);
        socket->recv_tail = (socket->recv_tail + chunk) % SOCKET_BUFFER_SIZE;
        written += chunk;
    }

    return (ssize_t)written;
}

static ssize_t socket_buffer_read(socket_endpoint_t *socket, void *buffer, size_t length) {
    u32 available = socket_buffer_used(socket);
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
        u32 chunk = (u32)SOCKET_BUFFER_SIZE - socket->recv_head;
        if (chunk > to_read - read) {
            chunk = to_read - read;
        }
        memcpy(dest + read, &socket->recv_buffer[socket->recv_head], chunk);
        socket->recv_head = (socket->recv_head + chunk) % SOCKET_BUFFER_SIZE;
        read += chunk;
    }

    return (ssize_t)read;
}

static socket_endpoint_t *socket_find_listener_by_path(const char *path) {
    for (int i = 0; i < SOCKET_MAX_OBJECTS; ++i) {
        socket_endpoint_t *sock = &socket_endpoints[i];
        if (!sock->used || !sock->listening || !sock->bound) {
            continue;
        }
        if (strcmp(sock->path, path) == 0) {
            return sock;
        }
    }
    return NULL;
}

void socket_init(void) {
    memset(socket_endpoints, 0, sizeof(socket_endpoints));
}

socket_endpoint_t *socket_create(int domain, int type, int protocol) {
    if (domain != AF_UNIX || type != SOCK_STREAM) {
        return NULL;
    }

    socket_endpoint_t *socket = socket_alloc();
    if (!socket) {
        return NULL;
    }

    socket->domain = domain;
    socket->type = type;
    socket->protocol = protocol;
    socket->bound = false;
    socket->peer_closed = false;
    socket->state = SOCKET_STATE_CREATED;
    socket_buffer_reset(socket);
    return socket;
}

int socket_bind(socket_endpoint_t *socket, const struct sockaddr *addr, socklen_t addrlen) {
    if (!socket || socket->bound || addrlen < sizeof(sa_family_t)) {
        return -EINVAL;
    }
    if (socket->domain != AF_UNIX || socket->type != SOCK_STREAM) {
        return -EAFNOSUPPORT;
    }

    const struct sockaddr_un *un = (const struct sockaddr_un *)addr;
    if (un->sun_family != AF_UNIX) {
        return -EAFNOSUPPORT;
    }

    bool abstract = false;
    if (un->sun_path[0] == '\0') {
        abstract = true;
    }
    if (!abstract && un->sun_path[0] == '\0') {
        return -EINVAL;
    }
    if (strlen(un->sun_path) >= sizeof(socket->path)) {
        return -ENAMETOOLONG;
    }

    if (!abstract) {
        socket_endpoint_t *existing = socket_find_listener_by_path(un->sun_path);
        if (existing) {
            return -EADDRINUSE;
        }
    }

    socket->bound = true;
    socket->abstract = abstract;
    if (abstract) {
        socket->path[0] = '\0';
        strcpy(socket->path + 1, un->sun_path + 1);
    } else {
        strcpy(socket->path, un->sun_path);
    }
    socket->state = SOCKET_STATE_BOUND;
    return 0;
}

int socket_listen(socket_endpoint_t *socket, int backlog) {
    if (!socket || !socket->bound || socket->type != SOCK_STREAM) {
        return -EINVAL;
    }
    if (backlog < 1) {
        backlog = 1;
    }
    if (backlog > SOCKET_BACKLOG) {
        backlog = SOCKET_BACKLOG;
    }

    socket->backlog = backlog;
    socket->listening = true;
    socket->state = SOCKET_STATE_LISTENING;
    socket->accept_waiter = -1;
    socket->pending_head = 0;
    socket->pending_tail = 0;
    return 0;
}

static int socket_pending_count(const socket_endpoint_t *socket) {
    if (socket->pending_tail >= socket->pending_head) {
        return socket->pending_tail - socket->pending_head;
    }
    return SOCKET_BACKLOG - socket->pending_head + socket->pending_tail;
}

int socket_pending_readable(const socket_endpoint_t *socket) {
    if (!socket) {
        return -EINVAL;
    }
    if (socket->listening) {
        return socket_pending_count(socket);
    }
    return (int)socket_buffer_used(socket);
}

int socket_enqueue_passed_node(socket_endpoint_t *socket, const vfs_node_t *node) {
    if (!socket || !node || socket_control_full(socket)) {
        return -EAGAIN;
    }
    socket->control_nodes[socket->control_tail] = node;
    socket->control_tail = (socket->control_tail + 1) % SOCKET_CONTROL_MAX;
    return 0;
}

int socket_dequeue_passed_node(socket_endpoint_t *socket, const vfs_node_t **node) {
    if (!socket || !node) {
        return -EINVAL;
    }
    if (socket_control_count(socket) == 0) {
        return -EAGAIN;
    }

    *node = socket->control_nodes[socket->control_head];
    socket->control_nodes[socket->control_head] = NULL;
    socket->control_head = (socket->control_head + 1) % SOCKET_CONTROL_MAX;
    return 0;
}

static bool socket_pending_full(const socket_endpoint_t *socket) {
    return socket_pending_count(socket) >= socket->backlog;
}

static bool socket_enqueue_pending(socket_endpoint_t *listener, int server_index) {
    if (socket_pending_full(listener)) {
        return false;
    }
    listener->pending[listener->pending_tail] = server_index;
    listener->pending_tail = (listener->pending_tail + 1) % SOCKET_BACKLOG;
    return true;
}

static int socket_dequeue_pending(socket_endpoint_t *listener) {
    if (socket_pending_count(listener) == 0) {
        return -1;
    }
    int server_index = listener->pending[listener->pending_head];
    listener->pending_head = (listener->pending_head + 1) % SOCKET_BACKLOG;
    return server_index;
}

int socket_accept_pending(socket_endpoint_t *listener, socket_endpoint_t **accepted) {
    if (!listener || !listener->listening || !accepted) {
        return -EINVAL;
    }
    int index = socket_dequeue_pending(listener);
    if (index < 0) {
        return -EAGAIN;
    }
    *accepted = &socket_endpoints[index];
    return 0;
}

int socket_connect(socket_endpoint_t *socket, const struct sockaddr *addr, socklen_t addrlen) {
    if (!socket || socket->peer) {
        return -EINVAL;
    }
    if (socket->domain != AF_UNIX || socket->type != SOCK_STREAM) {
        return -EAFNOSUPPORT;
    }
    if (addrlen < sizeof(sa_family_t)) {
        return -EINVAL;
    }

    const struct sockaddr_un *un = (const struct sockaddr_un *)addr;
    if (un->sun_family != AF_UNIX) {
        return -EAFNOSUPPORT;
    }
    if (un->sun_path[0] == '\0') {
        return -EAFNOSUPPORT;
    }

    socket_endpoint_t *listener = socket_find_listener_by_path(un->sun_path);
    if (!listener || !listener->listening) {
        return -ECONNREFUSED;
    }
    if (socket_pending_full(listener)) {
        return -EAGAIN;
    }

    socket_endpoint_t *server = socket_alloc();
    if (!server) {
        return -ENOMEM;
    }

    server->domain = socket->domain;
    server->type = socket->type;
    server->protocol = socket->protocol;
    server->state = SOCKET_STATE_CONNECTED;
    server->bound = false;
    server->peer_closed = false;
    socket_buffer_reset(server);
    server->peer = socket;
    socket->peer = server;
    socket->state = SOCKET_STATE_CONNECTED;
    socket->peer_closed = false;

    int server_index = (int)(server - socket_endpoints);
    if (!socket_enqueue_pending(listener, server_index)) {
        server->used = false;
        socket->peer = NULL;
        return -EAGAIN;
    }

    socket_try_complete_accept(listener);
    return 0;
}

ssize_t socket_recv(socket_endpoint_t *socket, void *buffer, size_t length, int flags) {
    if (!socket || socket->type != SOCK_STREAM) {
        return -ENOTCONN;
    }
    if (length == 0) {
        return 0;
    }
    ssize_t result;
    if ((flags & LINUX_MSG_PEEK) != 0) {
        u32 available = socket_buffer_used(socket);
        u32 to_read = (u32)length;
        if (to_read > available) {
            to_read = available;
        }
        if (to_read > 0) {
            u32 head = socket->recv_head;
            u8 *dest = (u8 *)buffer;
            u32 copied = 0;
            while (copied < to_read) {
                u32 chunk = (u32)SOCKET_BUFFER_SIZE - head;
                if (chunk > to_read - copied) {
                    chunk = to_read - copied;
                }
                memcpy(dest + copied, &socket->recv_buffer[head], chunk);
                head = (head + chunk) % SOCKET_BUFFER_SIZE;
                copied += chunk;
            }
            return (ssize_t)to_read;
        }
        result = 0;
    } else {
        result = socket_buffer_read(socket, buffer, length);
    }
    if (result > 0) {
        return result;
    }
    if (!socket->peer) {
        return 0;
    }
    if (socket->peer_closed) {
        return 0;
    }
    return -EAGAIN;
}

ssize_t socket_send(socket_endpoint_t *socket, const void *buffer, size_t length, int flags) {
    if (!socket || socket->type != SOCK_STREAM) {
        return -ENOTCONN;
    }
    if (!socket->peer) {
        return -ENOTCONN;
    }
    if (socket->peer->peer_closed) {
        return -EPIPE;
    }

    if (socket->peer->read_waiter >= 0 && socket->peer->waiting_for_read) {
        u32 to_copy = (u32)length;
        if (to_copy > socket->peer->read_length) {
            to_copy = socket->peer->read_length;
        }
        if (to_copy > 0) {
            /* Copy to kernel temp buffer first — buffer is a user-space
             * pointer in the SENDER's page directory, but task_write_slot_user
             * switches to the WAITER's page directory before memcpy.  Without
             * this intermediate copy we would read garbage or fault. */
            u8 *tmp = (u8 *)kmalloc(to_copy);
            if (tmp) {
                memcpy(tmp, buffer, to_copy);
                bool ok = task_write_slot_user(socket->peer->read_waiter,
                                               socket->peer->read_buffer, tmp, to_copy);
                kfree(tmp);
                if (ok) {
                    int slot = socket->peer->read_waiter;
                    socket->peer->read_waiter = -1;
                    socket->peer->waiting_for_read = false;
                    socket->peer->read_buffer = 0;
                    socket->peer->read_length = 0;
                    task_wake_slot(slot, (int)to_copy);

                    if ((size_t)to_copy == length) {
                        return (ssize_t)to_copy;
                    }
                    buffer = (const u8 *)buffer + to_copy;
                    length -= to_copy;
                }
            }
        }
    }

    ssize_t written = socket_buffer_write(socket->peer, buffer, length);
    if (written == 0) {
        return -EAGAIN;
    }
    if (written > 0 && socket->peer->read_waiter >= 0 && socket->peer->waiting_for_read) {
        task_wake_slot(socket->peer->read_waiter, (int)written);
        socket->peer->read_waiter = -1;
        socket->peer->waiting_for_read = false;
        socket->peer->read_buffer = 0;
        socket->peer->read_length = 0;
    }
    return written;
}

int socket_shutdown(socket_endpoint_t *socket, int how) {
    if (!socket) {
        return -EINVAL;
    }
    if (how == SHUT_WR || how == SHUT_RDWR) {
        if (socket->peer) {
            socket->peer->peer_closed = true;
            if (socket->peer->read_waiter >= 0) {
                task_wake_slot(socket->peer->read_waiter, 0);
                socket->peer->read_waiter = -1;
            }
        }
    }
    return 0;
}

int socket_getsockname(socket_endpoint_t *socket, struct sockaddr *addr, socklen_t *addrlen) {
    if (!socket || !addr || !addrlen) {
        return -EINVAL;
    }
    if (socket->domain != AF_UNIX) {
        return -EAFNOSUPPORT;
    }
    struct sockaddr_un local;
    memset(&local, 0, sizeof(local));
    local.sun_family = AF_UNIX;
    if (socket->bound && !socket->abstract) {
        strcpy(local.sun_path, socket->path);
    } else if (socket->bound && socket->abstract) {
        local.sun_path[0] = '\0';
        strcpy(local.sun_path + 1, socket->path + 1);
    }
    socklen_t needed = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + strlen(local.sun_path) + 1);
    if (*addrlen < needed) {
        return -EINVAL;
    }
    memcpy(addr, &local, needed);
    *addrlen = needed;
    return 0;
}

int socket_getpeername(socket_endpoint_t *socket, struct sockaddr *addr, socklen_t *addrlen) {
    if (!socket || !addr || !addrlen || !socket->peer) {
        return -ENOTCONN;
    }
    if (socket->domain != AF_UNIX) {
        return -EAFNOSUPPORT;
    }
    struct sockaddr_un peer;
    memset(&peer, 0, sizeof(peer));
    peer.sun_family = AF_UNIX;
    if (socket->peer->bound && !socket->peer->abstract) {
        strcpy(peer.sun_path, socket->peer->path);
    } else if (socket->peer->bound && socket->peer->abstract) {
        peer.sun_path[0] = '\0';
        strcpy(peer.sun_path + 1, socket->peer->path + 1);
    }
    socklen_t needed = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + strlen(peer.sun_path) + 1);
    if (*addrlen < needed) {
        return -EINVAL;
    }
    memcpy(addr, &peer, needed);
    *addrlen = needed;
    return 0;
}

int socket_setsockopt(socket_endpoint_t *socket, int level, int optname, const void *optval, socklen_t optlen) {
    (void)socket;
    (void)level;
    (void)optname;
    (void)optval;
    (void)optlen;
    return 0;
}

int socket_getsockopt(socket_endpoint_t *socket, int level, int optname, void *optval, socklen_t *optlen) {
    (void)socket;
    if (!optval || !optlen) {
        return -EINVAL;
    }
    if (level == SOL_SOCKET && optname == SO_ERROR) {
        if (*optlen < (socklen_t)sizeof(int)) {
            return -EINVAL;
        }
        *((int *)optval) = 0;
        *optlen = sizeof(int);
        return 0;
    }
    return -ENOPROTOOPT;
}

bool socket_unlink_path(const char *path) {
    for (int i = 0; i < SOCKET_MAX_OBJECTS; ++i) {
        socket_endpoint_t *sock = &socket_endpoints[i];
        if (!sock->used || !sock->bound) {
            continue;
        }
        if (strcmp(sock->path, path) == 0) {
            sock->bound = false;
            sock->path[0] = '\0';
            return true;
        }
    }
    return false;
}

int socket_stat_path(const char *path, struct linux_stat64 *statbuf) {
    for (int i = 0; i < SOCKET_MAX_OBJECTS; ++i) {
        socket_endpoint_t *sock = &socket_endpoints[i];
        if (!sock->used || !sock->bound) {
            continue;
        }
        if (strcmp(sock->path, path) == 0) {
            memset(statbuf, 0, sizeof(*statbuf));
            statbuf->st_mode = LINUX_S_IFSOCK | 0666u;
            statbuf->st_nlink = 1;
            statbuf->st_size = 0;
            statbuf->st_blksize = 1;
            return 0;
        }
    }
    return -ENOENT;
}

void socket_release(socket_endpoint_t *socket) {
    if (!socket || !socket->used) {
        return;
    }
    if (--socket->refcount > 0) {
        return;
    }
    if (socket->peer && socket->peer->peer == socket) {
        socket->peer->peer = NULL;
    }
    socket->bound = false;
    socket->path[0] = '\0';
    socket->used = false;
}
