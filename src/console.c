#include "console.h"
#include "console_font.h"

#include "format.h"
#include "io.h"
#include "memory.h"
#include "paging.h"
#include "serial.h"
#include "string.h"

#include "libtsm.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define FB_FONT_W CONSOLE_FONT_WIDTH
#define FB_FONT_H CONSOLE_FONT_HEIGHT

static volatile u16 *const vga_buffer = (u16 *)(KERNEL_VIRTUAL_BASE + 0xB8000);
static size_t cursor_row;
static size_t cursor_col;
static u8 console_color = (CONSOLE_LIGHT_GREY | (CONSOLE_BLACK << 4));

static char term_resp_buffer[CONSOLE_TERM_RESP_BUF_SIZE];
static volatile u32 term_resp_head;
static volatile u32 term_resp_tail;

static struct tsm_screen *tsm_scr;
static struct tsm_vte *tsm_v;

typedef struct {
    bool available;
    volatile u8 *framebuffer;
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
} framebuffer_terminal_t;

static framebuffer_terminal_t fb;

static void term_resp_queue_char(char ch) {
    u32 next = (term_resp_head + 1u) % CONSOLE_TERM_RESP_BUF_SIZE;
    if (next != term_resp_tail) {
        term_resp_buffer[term_resp_head] = ch;
        term_resp_head = next;
    }
}

static void term_resp_queue_string(const char *str) {
    while (*str) {
        term_resp_queue_char(*str++);
    }
}

int console_term_resp_available(void) {
    return term_resp_head != term_resp_tail ? 1 : 0;
}

size_t console_read_term_resp(char *buffer, size_t length) {
    size_t used = 0;
    while (used < length && term_resp_head != term_resp_tail) {
        buffer[used++] = term_resp_buffer[term_resp_tail];
        term_resp_tail = (term_resp_tail + 1u) % CONSOLE_TERM_RESP_BUF_SIZE;
    }
    return used;
}

static u16 vga_entry(char ch, u8 color) {
    return (u16)ch | ((u16)color << 8);
}

static void legacy_update_hw_cursor(void) {
    u16 pos = (u16)(cursor_row * VGA_WIDTH + cursor_col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (u8)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (u8)((pos >> 8) & 0xFF));
}

static void legacy_scroll_if_needed(void) {
    if (cursor_row < VGA_HEIGHT) return;
    memmove((void *)vga_buffer, (const void *)(vga_buffer + VGA_WIDTH),
            (VGA_HEIGHT - 1) * VGA_WIDTH * sizeof(u16));
    for (size_t col = 0; col < VGA_WIDTH; ++col)
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = vga_entry(' ', console_color);
    cursor_row = VGA_HEIGHT - 1;
}

static void legacy_clear(void) {
    cursor_row = 0;
    cursor_col = 0;
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i)
        vga_buffer[i] = vga_entry(' ', console_color);
    legacy_update_hw_cursor();
}

static void legacy_putc(char ch) {
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
            if (cursor_col > 0) cursor_col--;
            else if (cursor_row > 0) { cursor_row--; cursor_col = VGA_WIDTH - 1; }
            vga_buffer[cursor_row * VGA_WIDTH + cursor_col] = vga_entry(' ', console_color);
            break;
        default:
            vga_buffer[cursor_row * VGA_WIDTH + cursor_col] = vga_entry(ch, console_color);
            cursor_col++;
            if (cursor_col >= VGA_WIDTH) { cursor_col = 0; cursor_row++; }
            break;
    }
    legacy_scroll_if_needed();
    legacy_update_hw_cursor();
}

static void console_write_serial_char(char ch) {
    if (!serial_is_ready()) return;
    if (ch == '\n') serial_write_char('\r');
    serial_write_char(ch);
}

static u32 console_scale_channel(u8 value, u8 mask_size) {
    if (mask_size >= 8) return (u32)value << (mask_size - 8);
    if (mask_size == 0) return 0;
    return ((u32)value * ((1u << mask_size) - 1u) + 127u) / 255u;
}

static u32 console_pack_colour(u8 r, u8 g, u8 b) {
    return (console_scale_channel(r, fb.red_mask_size) << fb.red_shift) |
           (console_scale_channel(g, fb.green_mask_size) << fb.green_shift) |
           (console_scale_channel(b, fb.blue_mask_size) << fb.blue_shift);
}

static const u32 console_ansi_colours[16] = {
    0x000000, 0xAA0000, 0x00AA00, 0xAA5500,
    0x0000AA, 0xAA00AA, 0x00AAAA, 0xAAAAAA,
    0x555555, 0xFF5555, 0x55FF55, 0xFFFF55,
    0x5555FF, 0xFF55FF, 0x55FFFF, 0xFFFFFF,
};

static int console_draw_cb(struct tsm_screen *con, uint64_t id,
                           const uint32_t *ch, size_t len,
                           unsigned int width,
                           unsigned int posx, unsigned int posy,
                           const struct tsm_screen_attr *attr,
                           tsm_age_t age, void *data) {
    (void)con; (void)id; (void)age; (void)data;
    if (!fb.available) return 0;

    unsigned int font_w = FB_FONT_W;
    unsigned int font_h = FB_FONT_H;
    unsigned int bytes_pp = (fb.bpp + 7u) / 8u;

    unsigned int fg_idx = 15;
    unsigned int bg_idx = 0;
    bool fg_bright = false;

    if (attr->fccode >= 0 && (unsigned int)attr->fccode < 16) {
        fg_idx = (unsigned int)attr->fccode;
        fg_bright = (fg_idx >= 8);
        if (fg_bright) fg_idx -= 8;
    }
    if (attr->bccode >= 0 && (unsigned int)attr->bccode < 16)
        bg_idx = (unsigned int)attr->bccode;

    bool inv = attr->inverse;
    if (inv) {
        unsigned int tmp = fg_idx; fg_idx = bg_idx; bg_idx = tmp;
    }

    u32 fg_col = console_ansi_colours[fg_idx + (fg_bright ? 8 : 0)];
    u32 bg_col = console_ansi_colours[bg_idx];

    if (attr->bold && fg_idx < 8)
        fg_col = console_ansi_colours[fg_idx + 8];

    u32 fg_packed = console_pack_colour(
        (fg_col >> 16) & 0xFF, (fg_col >> 8) & 0xFF, fg_col & 0xFF);
    u32 bg_packed = console_pack_colour(
        (bg_col >> 16) & 0xFF, (bg_col >> 8) & 0xFF, bg_col & 0xFF);

    unsigned int glyph = 0;
    if (len > 0 && ch[0] < CONSOLE_FONT_GLYPHS)
        glyph = ch[0];

    for (unsigned int row = 0; row < font_h; ++row) {
        unsigned char font_byte = console_font[glyph * font_h + row];
        for (unsigned int col = 0; col < font_w; ++col) {
            int px = (int)(posx * font_w + col);
            int py = (int)(posy * font_h + row);
            if (px < 0 || (unsigned int)px >= fb.width) continue;
            if (py < 0 || (unsigned int)py >= fb.height) continue;

            bool set = (font_byte >> (7 - col)) & 1;
            u32 colour = set ? fg_packed : bg_packed;
            volatile u8 *pixel = fb.framebuffer + py * fb.pitch + px * bytes_pp;
            for (unsigned int b = 0; b < bytes_pp; ++b)
                pixel[b] = (u8)((colour >> (b * 8u)) & 0xFFu);
        }
    }
    return 0;
}

static void console_vte_write_cb(struct tsm_vte *vte, const char *u8,
                                  size_t len, void *data) {
    (void)vte; (void)data;
    for (size_t i = 0; i < len; ++i)
        term_resp_queue_char(u8[i]);
}

static void console_render(void) {
    if (tsm_scr)
        tsm_screen_draw(tsm_scr, console_draw_cb, NULL);
}

static void console_feed(const char *text, size_t length) {
    for (size_t i = 0; i < length; ++i)
        console_write_serial_char(text[i]);
    if (tsm_v) {
        tsm_vte_input(tsm_v, text, length);
        console_render();
    } else {
        for (size_t i = 0; i < length; ++i)
            legacy_putc(text[i]);
    }
}

void console_init(void) {
    tsm_scr = NULL;
    tsm_v = NULL;
    memset(&fb, 0, sizeof(fb));
    serial_init();
    legacy_clear();
}

void console_configure(const multiboot_info_t *mbi) {
    memset(&fb, 0, sizeof(fb));
    if (!mbi || !(mbi->flags & MULTIBOOT_FLAG_FRAMEBUFFER)) return;
    if (mbi->framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_RGB ||
        mbi->framebuffer_addr == 0 ||
        mbi->framebuffer_width == 0 ||
        mbi->framebuffer_height == 0 ||
        mbi->framebuffer_pitch == 0 ||
        mbi->framebuffer_bpp < 24 ||
        (mbi->framebuffer_addr >> 32) != 0) return;

    fb.available = true;
    fb.framebuffer = (volatile u8 *)(uintptr_t)mbi->framebuffer_addr;
    fb.width = mbi->framebuffer_width;
    fb.height = mbi->framebuffer_height;
    fb.pitch = mbi->framebuffer_pitch;
    fb.bpp = mbi->framebuffer_bpp;
    fb.red_mask_size = mbi->framebuffer_color_info.rgb.red_mask_size;
    fb.green_mask_size = mbi->framebuffer_color_info.rgb.green_mask_size;
    fb.blue_mask_size = mbi->framebuffer_color_info.rgb.blue_mask_size;
    fb.red_shift = mbi->framebuffer_color_info.rgb.red_field_position;
    fb.green_shift = mbi->framebuffer_color_info.rgb.green_field_position;
    fb.blue_shift = mbi->framebuffer_color_info.rgb.blue_field_position;

    if (fb.red_mask_size == 0 || fb.green_mask_size == 0 || fb.blue_mask_size == 0) {
        fb.red_shift = 16; fb.red_mask_size = 8;
        fb.green_shift = 8; fb.green_mask_size = 8;
        fb.blue_shift = 0; fb.blue_mask_size = 8;
    }
}

void console_activate(void) {
    if (tsm_scr || !fb.available) return;

    unsigned int cols = fb.width / FB_FONT_W;
    unsigned int rows = fb.height / FB_FONT_H;
    if (cols < 1 || rows < 1) return;

    if (tsm_screen_new(&tsm_scr, NULL, NULL) != 0) return;
    if (tsm_vte_new(&tsm_v, tsm_scr, console_vte_write_cb, NULL, NULL, NULL) != 0) {
        tsm_screen_unref(tsm_scr);
        tsm_scr = NULL;
        return;
    }

    tsm_screen_resize(tsm_scr, cols, rows);
    tsm_screen_set_max_sb(tsm_scr, 5000);
    tsm_screen_reset(tsm_scr);
    tsm_vte_input(tsm_v, "\x1b[20h", 5);

    console_clear();
    console_printf("[fb] %ux%u bpp=%u pitch=%u font=%ux%u cells=%ux%u\n",
                   fb.width, fb.height, fb.bpp, fb.pitch,
                   FB_FONT_W, FB_FONT_H, cols, rows);
}

void console_clear(void) {
    cursor_row = 0;
    cursor_col = 0;
    if (tsm_scr) {
        tsm_screen_erase_screen(tsm_scr, false);
        tsm_screen_move_to(tsm_scr, 0, 0);
        console_render();
        return;
    }
    legacy_clear();
}

void console_set_color(u8 fg, u8 bg) {
    console_color = (u8)(fg | (bg << 4));
}

void console_putc(char ch) {
    char buf[1] = {ch};
    console_feed(buf, 1);
}

void console_write(const char *text) {
    console_write_buffer(text, strlen(text));
}

void console_write_buffer(const char *text, size_t length) {
    if (!text || length == 0) return;
    console_feed(text, length);
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

int console_get_fb_info(console_fb_info_t *info) {
    if (!info || !fb.available) return 0;
    info->address = fb.framebuffer;
    info->width = fb.width;
    info->height = fb.height;
    info->pitch = fb.pitch;
    info->bpp = fb.bpp;
    info->red_mask_size = fb.red_mask_size;
    info->red_shift = fb.red_shift;
    info->green_mask_size = fb.green_mask_size;
    info->green_shift = fb.green_shift;
    info->blue_mask_size = fb.blue_mask_size;
    info->blue_shift = fb.blue_shift;
    return 1;
}

int console_get_dimensions(size_t *cols, size_t *rows) {
    if (!tsm_scr) return 0;
    *cols = tsm_screen_get_width(tsm_scr);
    *rows = tsm_screen_get_height(tsm_scr);
    return 1;
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
            if (j == 7) console_putc(' ');
        }
        ascii[line_len] = '\0';
        console_printf(" |%s|\n", ascii);
    }
}
