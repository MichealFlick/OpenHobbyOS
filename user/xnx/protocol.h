#ifndef XNX_PROTOCOL_H
#define XNX_PROTOCOL_H

#include <stdint.h>

#define XNX_SOCKET_PATH "/tmp/xnx.sock"
#define XNX_MAX_SURFACES 256
#define XNX_MAX_CLIENTS 32
#define XNX_BACKLOG 4
#define XNX_PROTOCOL_VERSION 1
#define XNX_MAX_MESSAGE_BYTES 65536

#define XNX_BUF_W 1024
#define XNX_BUF_H 768

enum xnx_message_type {
    XNX_CREATE_SURFACE  = 0x0001,
    XNX_DESTROY_SURFACE = 0x0002,
    XNX_SET_TITLE       = 0x0003,
    XNX_SET_GEOMETRY    = 0x0004,
    XNX_WRITE_BUFFER    = 0x0005,
    XNX_COMMIT          = 0x0006,
    XNX_GET_DISPLAY_INFO = 0x0007,
    XNX_SURFACE_CREATED = 0x8001,
    XNX_KEY_EVENT       = 0x8002,
    XNX_POINTER_EVENT   = 0x8003,
    XNX_CLOSE_REQUEST   = 0x8004,
    XNX_FRAME_DONE      = 0x8005,
    XNX_DISPLAY_INFO    = 0x8006,
};

struct xnx_header {
    uint32_t type;
    uint32_t payload_size;
};

struct xnx_create_surface {
    uint32_t width;
    uint32_t height;
};

struct xnx_surface_created {
    uint32_t surface_id;
};

struct xnx_display_info {
    uint32_t width;
    uint32_t height;
};

struct xnx_set_geometry {
    uint32_t surface_id;
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
};

struct xnx_write_buffer {
    uint32_t surface_id;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
};

struct xnx_commit {
    uint32_t surface_id;
};

struct xnx_key_event {
    uint32_t surface_id;
    uint32_t keycode;
    uint8_t pressed;
};

struct xnx_fb_bitfield {
    uint32_t offset;
    uint32_t length;
    uint32_t msb_right;
};

struct xnx_fb_var_screeninfo {
    uint32_t xres;
    uint32_t yres;
    uint32_t xres_virtual;
    uint32_t yres_virtual;
    uint32_t xoffset;
    uint32_t yoffset;
    uint32_t bits_per_pixel;
    uint32_t grayscale;
    struct xnx_fb_bitfield red;
    struct xnx_fb_bitfield green;
    struct xnx_fb_bitfield blue;
    struct xnx_fb_bitfield transp;
};

#define XNX_FBIOGET_VSCREENINFO 0x4600

#endif
