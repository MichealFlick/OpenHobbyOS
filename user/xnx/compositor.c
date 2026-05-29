#include <errno.h>
#include <fcntl.h>
#include <pixman.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>


#include "protocol.h"

#define MAX_SURFACES_PER_CLIENT 16
#define MAX_CLIENTS 32
#define MAX_SURFACES 256
#define READ_BUF_SIZE XNX_MAX_MESSAGE_BYTES
#define FRAME_INTERVAL_MS 33

struct xnx_surface {
    uint32_t id;
    int client_fd;
    pixman_image_t *image;
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    char title[64];
    int z_order;
    int visible;
};

struct xnx_client {
    int fd;
    uint32_t id;
    int num_surfaces;
    uint32_t surface_ids[MAX_SURFACES_PER_CLIENT];
    uint8_t read_buf[READ_BUF_SIZE];
    int read_offset;
    int msg_state;
    struct xnx_header current_header;
    int active;
};

static struct xnx_surface surfaces[MAX_SURFACES];
static int num_surfaces;
static uint32_t next_surface_id;
static int next_z_order = 1;

static struct xnx_client clients[MAX_CLIENTS];
static int num_clients;
static uint32_t next_client_id;

static int listener_fd = -1;
static pixman_image_t *framebuffer;
static int fb_width;
static int fb_height;
static size_t fb_size;
static uint32_t *fb_pixels;
static uint32_t *fb_hw_pixels;
static size_t fb_hw_pitch;
static int fb_bpp;
static uint32_t focused_surface_id;

static int send_all(int fd, const void *buffer, size_t length) {
    const uint8_t *cursor = (const uint8_t *)buffer;
    size_t remaining = length;

    while (remaining > 0) {
        ssize_t sent = send(fd, cursor, remaining, 0);
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
            usleep(1000);
            continue;
        }
        return -1;
    }

    return 0;
}

static int send_message(int fd, uint32_t type, const void *payload, uint32_t payload_size) {
    struct xnx_header hdr;

    hdr.type = type;
    hdr.payload_size = payload_size;

    if (send_all(fd, &hdr, sizeof(hdr)) < 0) {
        return -1;
    }
    if (payload_size > 0 && send_all(fd, payload, payload_size) < 0) {
        return -1;
    }

    return 0;
}

static struct xnx_surface *find_surface(uint32_t id) {
    for (int i = 0; i < num_surfaces; ++i) {
        if (surfaces[i].id == id) {
            return &surfaces[i];
        }
    }
    return NULL;
}

static struct xnx_surface *top_visible_surface(void) {
    struct xnx_surface *top = NULL;

    for (int i = 0; i < num_surfaces; ++i) {
        if (!surfaces[i].visible || !surfaces[i].image) {
            continue;
        }
        if (!top || surfaces[i].z_order > top->z_order) {
            top = &surfaces[i];
        }
    }

    return top;
}

static void ensure_focus(void) {
    struct xnx_surface *focused = find_surface(focused_surface_id);

    if (focused && focused->visible && focused->image) {
        return;
    }

    focused = top_visible_surface();
    focused_surface_id = focused ? focused->id : 0;
}

static void focus_surface(struct xnx_surface *surface) {
    if (!surface) {
        ensure_focus();
        return;
    }

    surface->z_order = next_z_order++;
    focused_surface_id = surface->id;
}

static void flush_fb(void) {
    if (!fb_hw_pixels || !fb_pixels || !fb_hw_pitch) {
        return;
    }
    for (int y = 0; y < fb_height; ++y) {
        memcpy((uint8_t *)fb_hw_pixels + (size_t)y * fb_hw_pitch,
               (uint8_t *)fb_pixels + (size_t)y * (size_t)fb_width * 4u,
               (size_t)fb_width * 4u < fb_hw_pitch ? (size_t)fb_width * 4u : fb_hw_pitch);
    }
}

static void drop_surface(uint32_t surface_id) {
    for (int i = 0; i < num_surfaces; ++i) {
        if (surfaces[i].id != surface_id) {
            continue;
        }
        if (surfaces[i].image) {
            pixman_image_unref(surfaces[i].image);
        }
        surfaces[i] = surfaces[--num_surfaces];
        break;
    }

    if (focused_surface_id == surface_id) {
        focused_surface_id = 0;
    }
    ensure_focus();
}

static void remove_client(int idx) {
    struct xnx_client *client = &clients[idx];

    for (int i = 0; i < client->num_surfaces; ++i) {
        drop_surface(client->surface_ids[i]);
    }

    if (client->fd >= 0) {
        close(client->fd);
    }

    memset(client, 0, sizeof(*client));
    clients[idx] = clients[--num_clients];

    if (num_surfaces == 0) {
    memcpy(fb_pixels, fb_hw_pixels, fb_size);
    }
}

static struct xnx_client *add_client(int fd) {
    struct xnx_client *client;

    if (num_clients >= MAX_CLIENTS) {
        return NULL;
    }

    client = &clients[num_clients++];
    memset(client, 0, sizeof(*client));
    client->fd = fd;
    client->id = ++next_client_id;
    client->active = 1;
    return client;
}

static int create_surface(struct xnx_client *client, uint32_t width, uint32_t height, uint32_t *out_id) {
    pixman_image_t *image;
    struct xnx_surface *surface;
    uint32_t id;

    if (!client || !out_id) {
        return -1;
    }
    if (client->num_surfaces >= MAX_SURFACES_PER_CLIENT || num_surfaces >= MAX_SURFACES) {
        return -1;
    }
    if (width == 0 || height == 0 || width > 4096 || height > 4096) {
        return -1;
    }

    image = pixman_image_create_bits(PIXMAN_a8r8g8b8, width, height, NULL, 0);
    if (!image) {
        return -1;
    }

    id = ++next_surface_id;
    surface = &surfaces[num_surfaces++];
    memset(surface, 0, sizeof(*surface));
    surface->id = id;
    surface->client_fd = client->fd;
    surface->image = image;
    surface->width = width;
    surface->height = height;
    surface->visible = 1;
    focus_surface(surface);

    client->surface_ids[client->num_surfaces++] = id;
    *out_id = id;
    return 0;
}

static void handle_set_geometry(struct xnx_client *client, struct xnx_set_geometry *geometry) {
    struct xnx_surface *surface = find_surface(geometry->surface_id);

    if (!surface || surface->client_fd != client->fd) {
        return;
    }

    surface->x = geometry->x;
    surface->y = geometry->y;
    if (geometry->width > 0 && geometry->height > 0 &&
        (geometry->width != surface->width || geometry->height != surface->height)) {
        pixman_image_t *new_image = pixman_image_create_bits(PIXMAN_a8r8g8b8, geometry->width, geometry->height, NULL, 0);
        if (new_image) {
            uint32_t copy_width = surface->width < geometry->width ? surface->width : geometry->width;
            uint32_t copy_height = surface->height < geometry->height ? surface->height : geometry->height;

            pixman_image_composite32(PIXMAN_OP_SRC, surface->image, NULL, new_image,
                0, 0, 0, 0, 0, 0, copy_width, copy_height);
            pixman_image_unref(surface->image);
            surface->image = new_image;
            surface->width = geometry->width;
            surface->height = geometry->height;
        }
    }
}

static void handle_set_title(struct xnx_client *client, uint32_t surface_id, const char *title) {
    struct xnx_surface *surface = find_surface(surface_id);

    if (!surface || surface->client_fd != client->fd) {
        return;
    }

    strncpy(surface->title, title, sizeof(surface->title) - 1);
    surface->title[sizeof(surface->title) - 1] = '\0';
}

static void handle_write_buffer(struct xnx_client *client, struct xnx_write_buffer *buffer, const uint8_t *pixels) {
    struct xnx_surface *surface = find_surface(buffer->surface_id);
    uint32_t width;
    uint32_t height;
    uint32_t *dst;
    uint32_t stride;

    if (!surface || surface->client_fd != client->fd) {
        return;
    }
    if (!surface->image) {
        return;
    }

    width = buffer->width;
    height = buffer->height;
    if (width == 0 || height == 0) {
        return;
    }
    if (buffer->x >= surface->width || buffer->y >= surface->height) {
        return;
    }
    if (width > surface->width - buffer->x) {
        width = surface->width - buffer->x;
    }
    if (height > surface->height - buffer->y) {
        height = surface->height - buffer->y;
    }

    dst = pixman_image_get_data(surface->image);
    stride = (uint32_t)pixman_image_get_stride(surface->image) / 4u;
    if (!dst || stride == 0) {
        return;
    }

    {
        size_t src_stride = (size_t)buffer->width * sizeof(uint32_t);
        for (uint32_t row = 0; row < height; ++row) {
            size_t dst_off = ((size_t)(buffer->y + row) * (size_t)stride + (size_t)buffer->x);
            size_t src_off = (size_t)row * (size_t)buffer->width;
            memcpy(&dst[dst_off], pixels + src_off * sizeof(uint32_t),
                   (size_t)width * sizeof(uint32_t));
        }
    }
}

static int send_key_to_focus(uint32_t keycode) {
    struct xnx_surface *surface;
    struct xnx_key_event event;

    ensure_focus();
    surface = find_surface(focused_surface_id);
    if (!surface) {
        return 0;
    }

    memset(&event, 0, sizeof(event));
    event.surface_id = surface->id;
    event.keycode = keycode;
    event.pressed = 1;
    return send_message(surface->client_fd, XNX_KEY_EVENT, &event, sizeof(event));
}

static void composite_surfaces(void) {
    int z = 0;

    if (!framebuffer || !fb_pixels) {
        return;
    }

    if (num_surfaces == 0) {
        return;
    }

    memcpy(fb_pixels, fb_hw_pixels, fb_size);

    for (;;) {
        struct xnx_surface *next = NULL;

        for (int i = 0; i < num_surfaces; ++i) {
            struct xnx_surface *surface = &surfaces[i];

            if (!surface->visible || !surface->image || surface->z_order < z) {
                continue;
            }
            if (!next || surface->z_order < next->z_order) {
                next = surface;
            }
        }

        if (!next) {
            break;
        }

        if (!next->image) {
            z = next->z_order + 1;
            continue;
        }

        {
            int dx = next->x;
            int dy = next->y;
            int sx = 0;
            int sy = 0;
            unsigned int surf_w = next->width;
            unsigned int surf_h = next->height;

            if (surf_w == 0 || surf_h == 0) {
                z = next->z_order + 1;
                continue;
            }

            int width = (int)surf_w;
            int height = (int)surf_h;

            if (dx < 0) {
                sx = -dx;
                width += dx;
                dx = 0;
            }
            if (dy < 0) {
                sy = -dy;
                height += dy;
                dy = 0;
            }
            if (dx >= fb_width || dy >= fb_height || sx >= (int)surf_w || sy >= (int)surf_h) {
                z = next->z_order + 1;
                continue;
            }
            if (dx + width > fb_width) {
                width = fb_width - dx;
            }
            if (dy + height > fb_height) {
                height = fb_height - dy;
            }
            if ((unsigned int)(sx + width) > surf_w) {
                width = (int)surf_w - sx;
            }
            if ((unsigned int)(sy + height) > surf_h) {
                height = (int)surf_h - sy;
            }
            if (width > 0 && height > 0) {
                pixman_image_composite32(PIXMAN_OP_OVER, next->image, NULL, framebuffer,
                    sx, sy, 0, 0, dx, dy, (unsigned int)width, (unsigned int)height);
            }
        }

        z = next->z_order + 1;
    }

    flush_fb();
}

static int setup_socket(void) {
    struct sockaddr_un addr;
    int fd;
    int flags;

    unlink(XNX_SOCKET_PATH);
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        fprintf(stderr, "socket failed\n");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, XNX_SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "bind failed\n");
        close(fd);
        return -1;
    }
    if (listen(fd, XNX_BACKLOG) < 0) {
        fprintf(stderr, "listen failed\n");
        close(fd);
        return -1;
    }

    flags = fcntl(fd, F_GETFD, 0);
    if (flags >= 0) {
        fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
    }

    return fd;
}

static int handle_client_message(struct xnx_client *client) {
    uint8_t *payload = client->read_buf;
    uint32_t type = client->current_header.type;
    uint32_t size = client->current_header.payload_size;

    switch (type) {
        case XNX_GET_DISPLAY_INFO: {
            struct xnx_display_info info;

            info.width = (uint32_t)fb_width;
            info.height = (uint32_t)fb_height;
            return send_message(client->fd, XNX_DISPLAY_INFO, &info, sizeof(info));
        }
        case XNX_CREATE_SURFACE: {
            struct xnx_create_surface *request;
            struct xnx_surface_created created;

            if (size < sizeof(*request)) {
                return -1;
            }
            request = (struct xnx_create_surface *)payload;
            if (create_surface(client, request->width, request->height, &created.surface_id) < 0) {
                return -1;
            }
            return send_message(client->fd, XNX_SURFACE_CREATED, &created, sizeof(created));
        }
        case XNX_DESTROY_SURFACE: {
            uint32_t surface_id;
            struct xnx_surface *surface;

            if (size < sizeof(surface_id)) {
                return -1;
            }
            memcpy(&surface_id, payload, sizeof(surface_id));
            surface = find_surface(surface_id);
            if (!surface || surface->client_fd != client->fd) {
                return -1;
            }
            drop_surface(surface_id);
            for (int i = 0; i < client->num_surfaces; ++i) {
                if (client->surface_ids[i] == surface_id) {
                    client->surface_ids[i] = client->surface_ids[--client->num_surfaces];
                    break;
                }
            }
            return 0;
        }
        case XNX_SET_TITLE:
            if (size < sizeof(uint32_t) + 1u) {
                return -1;
            }
            handle_set_title(client, *(uint32_t *)payload, (const char *)(payload + sizeof(uint32_t)));
            return 0;
        case XNX_SET_GEOMETRY:
            if (size < sizeof(struct xnx_set_geometry)) {
                return -1;
            }
            handle_set_geometry(client, (struct xnx_set_geometry *)payload);
            return 0;
        case XNX_WRITE_BUFFER: {
            struct xnx_write_buffer *buffer;
            size_t pixel_bytes;

            if (size < sizeof(struct xnx_write_buffer)) {
                return -1;
            }
            buffer = (struct xnx_write_buffer *)payload;
            pixel_bytes = (size_t)buffer->width * (size_t)buffer->height * sizeof(uint32_t);
            if (pixel_bytes > (size_t)size - sizeof(*buffer)) {
                return -1;
            }
            handle_write_buffer(client, buffer, payload + sizeof(*buffer));
            return 0;
        }
        case XNX_COMMIT: {
            uint32_t surface_id;
            struct xnx_surface *surface;

            if (size < sizeof(surface_id)) {
                return -1;
            }
            memcpy(&surface_id, payload, sizeof(surface_id));
            surface = find_surface(surface_id);
            if (!surface || surface->client_fd != client->fd) {
                return -1;
            }
            focus_surface(surface);
            return send_message(client->fd, XNX_FRAME_DONE, NULL, 0);
        }
        default:
            return -1;
    }
}

static int read_client_data(struct xnx_client *client) {
    ssize_t n;

    if (client->msg_state == 0) {
        int needed = (int)sizeof(struct xnx_header) - client->read_offset;

        n = recv(client->fd, (uint8_t *)&client->current_header + client->read_offset, (size_t)needed, 0);
        if (n == 0) {
            return -1;
        }
        if (n < 0) {
            return (errno == EINTR || errno == EAGAIN) ? 0 : -1;
        }
        client->read_offset += (int)n;
        if (client->read_offset == (int)sizeof(struct xnx_header)) {
            client->read_offset = 0;
            client->msg_state = 1;
            if (client->current_header.payload_size > sizeof(client->read_buf)) {
                return -1;
            }
            if (client->current_header.payload_size == 0) {
                client->msg_state = 0;
                return handle_client_message(client);
            }
        }
        return 0;
    }

    n = recv(client->fd,
             client->read_buf + client->read_offset,
             client->current_header.payload_size - (uint32_t)client->read_offset,
             0);
    if (n == 0) {
        return -1;
    }
    if (n < 0) {
        return (errno == EINTR || errno == EAGAIN) ? 0 : -1;
    }
    client->read_offset += (int)n;
    if (client->read_offset == (int)client->current_header.payload_size) {
        client->read_offset = 0;
        client->msg_state = 0;
        return handle_client_message(client);
    }

    return 0;
}

int main(void) {
    int fb_fd;
    struct xnx_fb_var_screeninfo vinfo;

    printf("XNX Compositor v0.2 starting...\n");
    fflush(stdout);

    listener_fd = setup_socket();
    if (listener_fd < 0) {
        return 1;
    }

    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        fprintf(stderr, "Failed to open /dev/fb0\n");
        return 1;
    }
    if (ioctl(fb_fd, XNX_FBIOGET_VSCREENINFO, &vinfo) < 0) {
        fprintf(stderr, "Failed to get framebuffer info\n");
        close(fb_fd);
        return 1;
    }

    fb_width = (int)vinfo.xres;
    fb_height = (int)vinfo.yres;
    fb_bpp = (int)vinfo.bits_per_pixel;
    if (fb_bpp < 1) fb_bpp = 32;
    fb_hw_pitch = (size_t)vinfo.xres_virtual * (size_t)vinfo.bits_per_pixel / 8u;
    if (fb_hw_pitch == 0) fb_hw_pitch = (size_t)fb_width * 4u;

    fb_size = (size_t)fb_width * (size_t)fb_height * 4u;
    fb_pixels = calloc(1, fb_size);
    if (!fb_pixels) {
        fprintf(stderr, "Failed to allocate compositing buffer\n");
        close(fb_fd);
        return 1;
    }

    size_t hw_map_size = fb_hw_pitch * (size_t)fb_height;
    fb_hw_pixels = mmap(NULL, hw_map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_hw_pixels == MAP_FAILED) {
        fprintf(stderr, "Failed to mmap framebuffer\n");
        free(fb_pixels);
        close(fb_fd);
        return 1;
    }

    framebuffer = pixman_image_create_bits(PIXMAN_a8r8g8b8, fb_width, fb_height, fb_pixels, fb_width * 4);
    if (!framebuffer) {
        fprintf(stderr, "Failed to create pixman framebuffer\n");
        munmap(fb_hw_pixels, hw_map_size);
        free(fb_pixels);
        close(fb_fd);
        return 1;
    }

    memset(fb_pixels, 0, fb_size);
    flush_fb();

    printf("Listening on %s (%dx%d)\n", XNX_SOCKET_PATH, fb_width, fb_height);

    for (;;) {
        struct pollfd fds[MAX_CLIENTS + 2];
        int client_slots[MAX_CLIENTS];
        int nfds = 0;
        int stdin_index = -1;
        int tracked_clients = 0;

        fds[nfds].fd = listener_fd;
        fds[nfds].events = POLLIN;
        fds[nfds].revents = 0;
        ++nfds;

        if (num_surfaces > 0) {
            stdin_index = nfds;
            fds[nfds].fd = STDIN_FILENO;
            fds[nfds].events = POLLIN;
            fds[nfds].revents = 0;
            ++nfds;
        }

        for (int i = 0; i < num_clients; ++i) {
            if (!clients[i].active) {
                continue;
            }
            client_slots[tracked_clients++] = i;
            fds[nfds].fd = clients[i].fd;
            fds[nfds].events = POLLIN;
            fds[nfds].revents = 0;
            ++nfds;
        }

        if (poll(fds, (nfds_t)nfds, FRAME_INTERVAL_MS) < 0 && errno != EINTR) {
            break;
        }

        if ((fds[0].revents & POLLIN) != 0) {
            struct sockaddr_un client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(listener_fd, (struct sockaddr *)&client_addr, &client_len);

            if (client_fd >= 0 && !add_client(client_fd)) {
                close(client_fd);
            }
        }

        if (stdin_index >= 0 && (fds[stdin_index].revents & POLLIN) != 0) {
            unsigned char ch;

            if (read(STDIN_FILENO, &ch, 1) == 1) {
                if (send_key_to_focus((uint32_t)ch) < 0) {
                    perror("send key event");
                }
            }
        }

        for (int i = 0, fd_index = (stdin_index >= 0 ? stdin_index + 1 : 1); i < tracked_clients; ++i, ++fd_index) {
            int client_index = client_slots[i];

            if (client_index >= num_clients || !clients[client_index].active) {
                continue;
            }
            if ((fds[fd_index].revents & (POLLIN | POLLHUP | POLLERR)) != 0) {
                if (read_client_data(&clients[client_index]) < 0) {
                    remove_client(client_index);
                }
            }
        }

        composite_surfaces();
    }

    if (listener_fd >= 0) {
        close(listener_fd);
    }
    if (framebuffer) {
        pixman_image_unref(framebuffer);
    }
    if (fb_hw_pixels && fb_hw_pixels != MAP_FAILED) {
        munmap(fb_hw_pixels, fb_hw_pitch * (size_t)fb_height);
    }
    if (fb_pixels) {
        free(fb_pixels);
    }
    close(fb_fd);
    unlink(XNX_SOCKET_PATH);
    return 0;
}
