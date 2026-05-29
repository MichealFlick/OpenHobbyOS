#include "xnx.h"

#include "../xnx/protocol.h"

#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

struct xnx_connection {
    int fd;
    uint8_t read_buf[XNX_MAX_MESSAGE_BYTES];
    struct xnx_event events[16];
    unsigned int event_head;
    unsigned int event_tail;
};

static int xnx_wait_briefly(void) {
    return usleep(1000);
}

static int xnx_send_all(xnx_conn_t *c, const void *buffer, size_t length) {
    const uint8_t *cursor = (const uint8_t *)buffer;
    size_t remaining = length;

    while (remaining > 0) {
        ssize_t sent = send(c->fd, cursor, remaining, 0);
        if (sent > 0) {
            cursor += (size_t)sent;
            remaining -= (size_t)sent;
            continue;
        }
        if (sent == 0) {
            errno = EPIPE;
            return -1;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN) {
            if (xnx_wait_briefly() != 0 && errno != EINTR) {
                return -1;
            }
            continue;
        }
        return -1;
    }

    return 0;
}

static int xnx_recv_all(xnx_conn_t *c, void *buffer, size_t length) {
    uint8_t *cursor = (uint8_t *)buffer;
    size_t remaining = length;

    while (remaining > 0) {
        ssize_t received = recv(c->fd, cursor, remaining, 0);
        if (received > 0) {
            cursor += (size_t)received;
            remaining -= (size_t)received;
            continue;
        }
        if (received == 0) {
            errno = ECONNRESET;
            return -1;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN) {
            if (xnx_wait_briefly() != 0 && errno != EINTR) {
                return -1;
            }
            continue;
        }
        return -1;
    }

    return 0;
}

static int xnx_send_message(xnx_conn_t *c, uint32_t type, const void *payload, uint32_t payload_size) {
    struct xnx_header hdr;

    if (!c) {
        errno = EINVAL;
        return -1;
    }
    if ((size_t)payload_size > sizeof(c->read_buf)) {
        errno = EMSGSIZE;
        return -1;
    }

    hdr.type = type;
    hdr.payload_size = payload_size;

    if (xnx_send_all(c, &hdr, sizeof(hdr)) < 0) {
        return -1;
    }
    if (payload_size > 0 && xnx_send_all(c, payload, payload_size) < 0) {
        return -1;
    }

    return 0;
}

static int xnx_recv_message(xnx_conn_t *c, struct xnx_header *hdr, void *payload, size_t payload_capacity) {
    if (!c || !hdr) {
        errno = EINVAL;
        return -1;
    }

    if (xnx_recv_all(c, hdr, sizeof(*hdr)) < 0) {
        return -1;
    }
    if ((size_t)hdr->payload_size > payload_capacity) {
        errno = EMSGSIZE;
        return -1;
    }
    if (hdr->payload_size > 0 && xnx_recv_all(c, payload, hdr->payload_size) < 0) {
        return -1;
    }

    return 0;
}

static int xnx_queue_event(xnx_conn_t *c, const struct xnx_event *event) {
    unsigned int next;

    if (!c || !event) {
        errno = EINVAL;
        return -1;
    }

    next = (c->event_head + 1u) % (sizeof(c->events) / sizeof(c->events[0]));
    if (next == c->event_tail) {
        errno = ENOBUFS;
        return -1;
    }

    c->events[c->event_head] = *event;
    c->event_head = next;
    return 0;
}

static int xnx_dequeue_event(xnx_conn_t *c, struct xnx_event *event) {
    if (!c || c->event_head == c->event_tail) {
        return 0;
    }

    if (event) {
        *event = c->events[c->event_tail];
    }
    c->event_tail = (c->event_tail + 1u) % (sizeof(c->events) / sizeof(c->events[0]));
    return 1;
}

static int xnx_handle_async_message(xnx_conn_t *c, const struct xnx_header *hdr) {
    struct xnx_event event;
    struct xnx_key_event *key;

    if (!c || !hdr) {
        errno = EINVAL;
        return -1;
    }

    if (hdr->type == XNX_KEY_EVENT) {
        if (hdr->payload_size != sizeof(struct xnx_key_event)) {
            errno = EPROTO;
            return -1;
        }
        key = (struct xnx_key_event *)c->read_buf;
        memset(&event, 0, sizeof(event));
        event.type = XNX_EVENT_KEY;
        event.data.key.surface_id = key->surface_id;
        event.data.key.keycode = key->keycode;
        event.data.key.pressed = key->pressed;
        return xnx_queue_event(c, &event);
    }

    if (hdr->type == XNX_FRAME_DONE && hdr->payload_size == 0) {
        return 0;
    }

    errno = EPROTO;
    return -1;
}

xnx_conn_t *xnx_connect(const char *socket_path) {
    int fd;
    struct sockaddr_un addr;
    xnx_conn_t *c;

    if (!socket_path) {
        socket_path = XNX_SOCKET_PATH;
    }

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return NULL;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return NULL;
    }

    c = calloc(1, sizeof(*c));
    if (!c) {
        close(fd);
        return NULL;
    }
    c->fd = fd;
    return c;
}

void xnx_disconnect(xnx_conn_t *c) {
    if (!c) {
        return;
    }
    if (c->fd >= 0) {
        close(c->fd);
    }
    free(c);
}

int xnx_get_display_info(xnx_conn_t *c, uint32_t *out_width, uint32_t *out_height) {
    struct xnx_header hdr;
    struct xnx_display_info info;

    if (xnx_send_message(c, XNX_GET_DISPLAY_INFO, NULL, 0) < 0) {
        return -1;
    }
    if (xnx_recv_message(c, &hdr, &info, sizeof(info)) < 0) {
        return -1;
    }
    if (hdr.type != XNX_DISPLAY_INFO || hdr.payload_size != sizeof(info)) {
        errno = EPROTO;
        return -1;
    }

    if (out_width) {
        *out_width = info.width;
    }
    if (out_height) {
        *out_height = info.height;
    }
    return 0;
}

int xnx_create_surface(xnx_conn_t *c, uint32_t width, uint32_t height, uint32_t *out_id) {
    struct xnx_create_surface cs;
    struct xnx_header hdr;
    struct xnx_surface_created sc;

    cs.width = width;
    cs.height = height;

    if (xnx_send_message(c, XNX_CREATE_SURFACE, &cs, sizeof(cs)) < 0) {
        return -1;
    }
    if (xnx_recv_message(c, &hdr, &sc, sizeof(sc)) < 0) {
        return -1;
    }
    if (hdr.type != XNX_SURFACE_CREATED || hdr.payload_size != sizeof(sc)) {
        errno = EPROTO;
        return -1;
    }

    if (out_id) {
        *out_id = sc.surface_id;
    }
    return 0;
}

int xnx_destroy_surface(xnx_conn_t *c, uint32_t surface_id) {
    return xnx_send_message(c, XNX_DESTROY_SURFACE, &surface_id, sizeof(surface_id));
}

int xnx_set_title(xnx_conn_t *c, uint32_t surface_id, const char *title) {
    size_t title_len;
    size_t total_size;
    uint8_t *buf;
    int ret = -1;

    if (!title) {
        errno = EINVAL;
        return -1;
    }

    title_len = strlen(title) + 1;
    total_size = sizeof(surface_id) + title_len;
    if (total_size > XNX_MAX_MESSAGE_BYTES) {
        errno = EMSGSIZE;
        return -1;
    }

    buf = malloc(total_size);
    if (!buf) {
        return -1;
    }

    memcpy(buf, &surface_id, sizeof(surface_id));
    memcpy(buf + sizeof(surface_id), title, title_len);
    ret = xnx_send_message(c, XNX_SET_TITLE, buf, (uint32_t)total_size);

    free(buf);
    return ret;
}

int xnx_set_geometry(xnx_conn_t *c, uint32_t surface_id, int32_t x, int32_t y, uint32_t width, uint32_t height) {
    struct xnx_set_geometry sg;

    sg.surface_id = surface_id;
    sg.x = x;
    sg.y = y;
    sg.width = width;
    sg.height = height;

    return xnx_send_message(c, XNX_SET_GEOMETRY, &sg, sizeof(sg));
}

int xnx_write_buffer(xnx_conn_t *c, uint32_t surface_id, uint32_t x, uint32_t y, uint32_t width, uint32_t height, const uint32_t *pixels) {
    const size_t max_pixel_bytes = XNX_MAX_MESSAGE_BYTES - sizeof(struct xnx_write_buffer);
    const size_t row_bytes = (size_t)width * sizeof(uint32_t);
    const uint8_t *source = (const uint8_t *)pixels;
    uint32_t rows_per_chunk;
    uint8_t *buf;
    uint32_t row = 0;
    int ret = 0;

    if (!c || (!pixels && width != 0 && height != 0)) {
        errno = EINVAL;
        return -1;
    }
    if (width == 0 || height == 0) {
        return 0;
    }
    if (row_bytes == 0 || row_bytes > max_pixel_bytes) {
        errno = EMSGSIZE;
        return -1;
    }

    rows_per_chunk = (uint32_t)(max_pixel_bytes / row_bytes);
    if (rows_per_chunk == 0) {
        errno = EMSGSIZE;
        return -1;
    }

    buf = malloc(sizeof(struct xnx_write_buffer) + (size_t)rows_per_chunk * row_bytes);
    if (!buf) {
        return -1;
    }

    while (row < height) {
        struct xnx_write_buffer *wb = (struct xnx_write_buffer *)buf;
        uint32_t chunk_rows = height - row;
        size_t pixel_bytes;

        if (chunk_rows > rows_per_chunk) {
            chunk_rows = rows_per_chunk;
        }

        wb->surface_id = surface_id;
        wb->x = x;
        wb->y = y + row;
        wb->width = width;
        wb->height = chunk_rows;

        pixel_bytes = (size_t)chunk_rows * row_bytes;
        memcpy(buf + sizeof(*wb), source + (size_t)row * row_bytes, pixel_bytes);

        if (xnx_send_message(c, XNX_WRITE_BUFFER, buf, (uint32_t)(sizeof(*wb) + pixel_bytes)) < 0) {
            ret = -1;
            break;
        }

        row += chunk_rows;
    }

    free(buf);
    return ret;
}

int xnx_commit(xnx_conn_t *c, uint32_t surface_id) {
    struct xnx_header hdr;

    if (xnx_send_message(c, XNX_COMMIT, &surface_id, sizeof(surface_id)) < 0) {
        return -1;
    }

    for (;;) {
        if (xnx_recv_message(c, &hdr, c->read_buf, sizeof(c->read_buf)) < 0) {
            return -1;
        }
        if (hdr.type == XNX_FRAME_DONE && hdr.payload_size == 0) {
            break;
        }
        if (xnx_handle_async_message(c, &hdr) < 0) {
            return -1;
        }
    }

    return 0;
}

int xnx_get_fd(xnx_conn_t *c) {
    return c ? c->fd : -1;
}

int xnx_poll_event(xnx_conn_t *c, struct xnx_event *out_event) {
    struct pollfd pfd;
    struct xnx_header hdr;
    int queued;

    if (!c) {
        errno = EINVAL;
        return -1;
    }

    queued = xnx_dequeue_event(c, out_event);
    if (queued != 0) {
        return queued;
    }

    pfd.fd = c->fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    if (poll(&pfd, 1, 0) <= 0 || (pfd.revents & POLLIN) == 0) {
        return 0;
    }

    if (xnx_recv_message(c, &hdr, c->read_buf, sizeof(c->read_buf)) < 0) {
        return -1;
    }
    if (xnx_handle_async_message(c, &hdr) < 0) {
        return -1;
    }

    return xnx_dequeue_event(c, out_event);
}

int xnx_dispatch(xnx_conn_t *c) {
    return xnx_poll_event(c, NULL);
}
