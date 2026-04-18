#ifndef OHOS_CONSOLE_H
#define OHOS_CONSOLE_H

#include <stdarg.h>

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

void console_init(void);
void console_clear(void);
void console_set_color(u8 fg, u8 bg);
void console_putc(char ch);
void console_write(const char *text);
void console_write_buffer(const char *text, size_t length);
void console_printf(const char *fmt, ...);
void console_vprintf(const char *fmt, va_list args);
void console_hexdump(const void *data, size_t length, uintptr_t base_offset);

#endif
