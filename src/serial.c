#include "serial.h"

#include "io.h"

#define COM1 0x3F8

static bool serial_ready;

void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
    outb(COM1 + 4, 0x1E);
    outb(COM1 + 0, 0xAE);

    serial_ready = inb(COM1 + 0) == 0xAE;

    outb(COM1 + 4, 0x0F);
}

bool serial_is_ready(void) {
    return serial_ready;
}

static bool serial_transmit_empty(void) {
    return (inb(COM1 + 5) & 0x20u) != 0;
}

bool serial_can_read(void) {
    return serial_ready && ((inb(COM1 + 5) & 0x01u) != 0);
}

char serial_read_char(void) {
    if (!serial_can_read()) {
        return 0;
    }
    return (char)inb(COM1);
}

void serial_write_char(char ch) {
    if (!serial_ready) {
        return;
    }
    while (!serial_transmit_empty()) {
    }
    outb(COM1, (u8)ch);
}

void serial_write(const char *text) {
    while (*text) {
        serial_write_char(*text++);
    }
}

void serial_write_buffer(const char *text, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        serial_write_char(text[i]);
    }
}
