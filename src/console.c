#include "console.h"

#include "format.h"
#include "io.h"
#include "serial.h"
#include "string.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

static volatile u16 *const vga_buffer = (u16 *)0xB8000;
static size_t cursor_row;
static size_t cursor_col;
static u8 console_color = (CONSOLE_LIGHT_GREY | (CONSOLE_BLACK << 4));

static u16 vga_entry(char ch, u8 color) {
    return (u16)ch | ((u16)color << 8);
}

static void update_hw_cursor(void) {
    u16 pos = (u16)(cursor_row * VGA_WIDTH + cursor_col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (u8)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (u8)((pos >> 8) & 0xFF));
}

static void scroll_if_needed(void) {
    if (cursor_row < VGA_HEIGHT) {
        return;
    }

    memmove((void *)vga_buffer, (const void *)(vga_buffer + VGA_WIDTH), (VGA_HEIGHT - 1) * VGA_WIDTH * sizeof(u16));
    for (size_t col = 0; col < VGA_WIDTH; ++col) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = vga_entry(' ', console_color);
    }
    cursor_row = VGA_HEIGHT - 1;
}

void console_init(void) {
    serial_init();
    console_clear();
}

void console_clear(void) {
    cursor_row = 0;
    cursor_col = 0;
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i) {
        vga_buffer[i] = vga_entry(' ', console_color);
    }
    update_hw_cursor();
}

void console_set_color(u8 fg, u8 bg) {
    console_color = (u8)(fg | (bg << 4));
}

void console_putc(char ch) {
    if (serial_is_ready()) {
        if (ch == '\n') {
            serial_write_char('\r');
        }
        serial_write_char(ch);
    }

    switch (ch) {
        case '\n':
            cursor_col = 0;
            cursor_row++;
            break;
        case '\r':
            cursor_col = 0;
            break;
        case '\t':
            cursor_col = (cursor_col + 4u) & ~3u;
            if (cursor_col >= VGA_WIDTH) {
                cursor_col = 0;
                cursor_row++;
            }
            break;
        case '\b':
            if (cursor_col > 0) {
                cursor_col--;
            } else if (cursor_row > 0) {
                cursor_row--;
                cursor_col = VGA_WIDTH - 1;
            }
            vga_buffer[cursor_row * VGA_WIDTH + cursor_col] = vga_entry(' ', console_color);
            break;
        default:
            vga_buffer[cursor_row * VGA_WIDTH + cursor_col] = vga_entry(ch, console_color);
            cursor_col++;
            if (cursor_col >= VGA_WIDTH) {
                cursor_col = 0;
                cursor_row++;
            }
            break;
    }

    scroll_if_needed();
    update_hw_cursor();
}

void console_write(const char *text) {
    while (*text) {
        console_putc(*text++);
    }
}

void console_write_buffer(const char *text, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        console_putc(text[i]);
    }
}

void console_vprintf(const char *fmt, va_list args) {
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    console_write(buffer);
}

void console_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    console_vprintf(fmt, args);
    va_end(args);
}

void console_hexdump(const void *data, size_t length, uintptr_t base_offset) {
    const u8 *bytes = (const u8 *)data;
    char ascii[17];
    ascii[16] = '\0';

    for (size_t i = 0; i < length; i += 16) {
        size_t line_len = (length - i > 16) ? 16 : (length - i);
        console_printf("%08x  ", (u32)(base_offset + i));

        for (size_t j = 0; j < 16; ++j) {
            if (j < line_len) {
                u8 value = bytes[i + j];
                console_printf("%02x ", value);
                ascii[j] = (value >= 32 && value <= 126) ? (char)value : '.';
            } else {
                console_write("   ");
                ascii[j] = ' ';
            }
            if (j == 7) {
                console_putc(' ');
            }
        }
        ascii[line_len] = '\0';
        console_printf(" |%s|\n", ascii);
    }
}
