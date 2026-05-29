#ifndef XNX_CLIENT_H
#define XNX_CLIENT_H

#include <stdint.h>
#include <stddef.h>

struct xnx_connection;
typedef struct xnx_connection xnx_conn_t;

enum xnx_event_type {
    XNX_EVENT_NONE = 0,
    XNX_EVENT_KEY = 1,
};

struct xnx_event {
    uint32_t type;
    union {
        struct {
            uint32_t surface_id;
            uint32_t keycode;
            uint8_t pressed;
        } key;
    } data;
};

xnx_conn_t *xnx_connect(const char *socket_path);
void xnx_disconnect(xnx_conn_t *c);

int xnx_get_display_info(xnx_conn_t *c, uint32_t *out_width, uint32_t *out_height);
int xnx_create_surface(xnx_conn_t *c, uint32_t width, uint32_t height, uint32_t *out_id);
int xnx_destroy_surface(xnx_conn_t *c, uint32_t surface_id);
int xnx_set_title(xnx_conn_t *c, uint32_t surface_id, const char *title);
int xnx_set_geometry(xnx_conn_t *c, uint32_t surface_id, int32_t x, int32_t y, uint32_t width, uint32_t height);
int xnx_write_buffer(xnx_conn_t *c, uint32_t surface_id, uint32_t x, uint32_t y, uint32_t width, uint32_t height, const uint32_t *pixels);
int xnx_commit(xnx_conn_t *c, uint32_t surface_id);

int xnx_get_fd(xnx_conn_t *c);
int xnx_poll_event(xnx_conn_t *c, struct xnx_event *out_event);
int xnx_dispatch(xnx_conn_t *c);

#endif
