#ifndef OHOS_CONSOLE_H
#define OHOS_CONSOLE_H

#include <stdarg.h>
#include <stdint.h>

#include "multiboot.h"
#include "types.h"

enum {
    CONSOLE_BLACK = 0,
    CONSOLE_BLUE = 1,
    CONSOLE_GREEN = 2,
    CONSOLE_CYAN = 3,
    CONSOLE_RED = 4,
    CONSOLE_MAGENTA = 5,
    CONSOLE_BROWN = 6,
    CONSOLE_LIGHT_GREY = 7,
    CONSOLE_DARK_GREY = 8,
    CONSOLE_LIGHT_BLUE = 9,
    CONSOLE_LIGHT_GREEN = 10,
    CONSOLE_LIGHT_CYAN = 11,
    CONSOLE_LIGHT_RED = 12,
    CONSOLE_PINK = 13,
    CONSOLE_YELLOW = 14,
    CONSOLE_WHITE = 15,
};

typedef struct {
    volatile u8 *address;
    u32 width;
    u32 height;
    u32 pitch;
    u8 bpp;
    u8 red_mask_size;
    u8 red_shift;
    u8 green_mask_size;
    u8 green_shift;
    u8 blue_mask_size;
    u8 blue_shift;
} console_fb_info_t;

void console_init(void);
void console_configure(const multiboot_info_t *mbi);
void console_activate(void);
void console_clear(void);
void console_set_color(u8 fg, u8 bg);
void console_putc(char ch);
void console_write(const char *text);
void console_write_buffer(const char *text, size_t length);
void console_printf(const char *fmt, ...);
void console_vprintf(const char *fmt, va_list args);
void console_hexdump(const void *data, size_t length, uintptr_t base_offset);
int console_get_fb_info(console_fb_info_t *info);
int console_get_dimensions(size_t *cols, size_t *rows);

#define CONSOLE_TERM_RESP_BUF_SIZE 256
int console_term_resp_available(void);
size_t console_read_term_resp(char *buffer, size_t length);

#endif
