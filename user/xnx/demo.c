#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "xnx.h"

static uint32_t make_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static void fill_gradient(uint32_t *pixels, uint32_t width, uint32_t height, uint32_t frame) {
    uint32_t shift = (frame * 3u) % 256u;

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint8_t r = (uint8_t)((x + shift) * 255u / (width ? width : 1u));
            uint8_t g = (uint8_t)((y * 255u) / (height ? height : 1u));
            uint8_t b = (uint8_t)(96u + ((x ^ y ^ frame) & 0x3fu));
            pixels[y * width + x] = make_color(r, g, b, 255);
        }
    }
}

static int handle_events(xnx_conn_t *conn, uint32_t *tint) {
    struct xnx_event event;

    while (xnx_poll_event(conn, &event) > 0) {
        if (event.type != XNX_EVENT_KEY || !event.data.key.pressed) {
            continue;
        }

        switch ((int)event.data.key.keycode) {
            case 3:
            case 'q':
            case 'Q':
            case 27:
                return 1;
            case ' ':
                *tint = (*tint + 37u) & 0xFFu;
                break;
            default:
                break;
        }
    }

    return 0;
}

int main(void) {
    xnx_conn_t *conn;
    uint32_t surf_id = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t *pixels = NULL;
    uint32_t frame = 0;
    uint32_t tint = 0;
    int exit_code = 1;

    printf("XNX Demo starting...\n");

    conn = xnx_connect(NULL);
    if (!conn) {
        printf("Failed to connect to XNX compositor\n");
        return 1;
    }
    printf("Connected to XNX\n");

    if (xnx_get_display_info(conn, &width, &height) < 0 || width == 0 || height == 0) {
        width = 1024;
        height = 768;
        printf("Falling back to %ux%u surface\n", width, height);
    } else {
        printf("Display is %ux%u\n", width, height);
    }

    if (xnx_create_surface(conn, width, height, &surf_id) < 0) {
        printf("Failed to create surface\n");
        xnx_disconnect(conn);
        return 1;
    }
    printf("Created fullscreen surface %u\n", surf_id);

    xnx_set_title(conn, surf_id, "XNX Demo");
    xnx_set_geometry(conn, surf_id, 0, 0, width, height);

    pixels = malloc((size_t)width * (size_t)height * sizeof(*pixels));
    if (!pixels) {
        printf("Out of memory\n");
        goto cleanup;
    }

    printf("Press q, Esc, or Ctrl+C to exit. Press space to shift the palette.\n");

    while (!handle_events(conn, &tint)) {
        fill_gradient(pixels, width, height, frame++);
        for (uint32_t i = 0; i < width * height; ++i) {
            pixels[i] ^= tint << 8;
        }

        if (xnx_write_buffer(conn, surf_id, 0, 0, width, height, pixels) < 0) {
            printf("Write buffer failed\n");
            goto cleanup;
        }
        if (xnx_commit(conn, surf_id) < 0) {
            printf("Commit failed\n");
            goto cleanup;
        }

        usleep(16000);
    }

    exit_code = 0;

cleanup:
    free(pixels);
    if (surf_id != 0) {
        xnx_destroy_surface(conn, surf_id);
    }
    xnx_disconnect(conn);
    printf("XNX Demo done\n");
    return exit_code;
}
