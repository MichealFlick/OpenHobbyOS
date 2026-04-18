#include "format.h"

#include "string.h"

static void append_char(char **cursor, char *end, char ch, int *written) {
    if (*cursor < end) {
        **cursor = ch;
        (*cursor)++;
    }
    (*written)++;
}

static void append_text(char **cursor, char *end, const char *text, int *written) {
    while (*text) {
        append_char(cursor, end, *text++, written);
    }
}

static size_t format_uint(char *buffer, u32 value, u32 base, bool uppercase) {
    size_t used = 0;
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

    if (value == 0) {
        buffer[used++] = '0';
        return used;
    }

    while (value) {
        buffer[used++] = digits[value % base];
        value /= base;
    }

    for (size_t i = 0; i < used / 2; ++i) {
        char tmp = buffer[i];
        buffer[i] = buffer[used - i - 1];
        buffer[used - i - 1] = tmp;
    }

    return used;
}

static void append_padded(char **cursor, char *end, const char *text, size_t length, int width, char pad, int *written) {
    while (width > (int)length) {
        append_char(cursor, end, pad, written);
        width--;
    }
    for (size_t i = 0; i < length; ++i) {
        append_char(cursor, end, text[i], written);
    }
}

static void append_uint(char **cursor, char *end, u32 value, u32 base, bool uppercase, int width, char pad, int *written) {
    char buffer[32];
    size_t used = format_uint(buffer, value, base, uppercase);
    append_padded(cursor, end, buffer, used, width, pad, written);
}

static void append_int(char **cursor, char *end, i32 value, int width, char pad, int *written) {
    char buffer[32];
    size_t used;

    if (value < 0) {
        append_char(cursor, end, '-', written);
        used = format_uint(buffer, (u32)(0u - (u32)value), 10, false);
        append_padded(cursor, end, buffer, used, width > 0 ? width - 1 : width, pad, written);
        return;
    }
    used = format_uint(buffer, (u32)value, 10, false);
    append_padded(cursor, end, buffer, used, width, pad, written);
}

int vsnprintf(char *buffer, size_t size, const char *fmt, va_list args) {
    char sink;
    char *cursor;
    char *end;
    int written = 0;

    if (size == 0) {
        buffer = &sink;
        size = 1;
    }

    cursor = buffer;
    end = buffer + size - 1;

    while (*fmt) {
        if (*fmt != '%') {
            append_char(&cursor, end, *fmt++, &written);
            continue;
        }

        fmt++;
        int width = 0;
        char pad = ' ';
        if (*fmt == '0') {
            pad = '0';
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }
        switch (*fmt) {
            case '%':
                append_char(&cursor, end, '%', &written);
                break;
            case 'c':
                append_char(&cursor, end, (char)va_arg(args, int), &written);
                break;
            case 's': {
                const char *text = va_arg(args, const char *);
                const char *safe = text ? text : "(null)";
                append_padded(&cursor, end, safe, strlen(safe), width, pad, &written);
                break;
            }
            case 'd':
            case 'i':
                append_int(&cursor, end, va_arg(args, i32), width, pad, &written);
                break;
            case 'u':
                append_uint(&cursor, end, va_arg(args, u32), 10, false, width, pad, &written);
                break;
            case 'x':
                append_uint(&cursor, end, va_arg(args, u32), 16, false, width, pad, &written);
                break;
            case 'X':
                append_uint(&cursor, end, va_arg(args, u32), 16, true, width, pad, &written);
                break;
            case 'p':
                append_text(&cursor, end, "0x", &written);
                append_uint(&cursor, end, va_arg(args, u32), 16, false, width ? width : 8, '0', &written);
                break;
            default:
                append_char(&cursor, end, '%', &written);
                append_char(&cursor, end, *fmt, &written);
                break;
        }
        if (*fmt) {
            fmt++;
        }
    }

    *cursor = '\0';
    return written;
}

int snprintf(char *buffer, size_t size, const char *fmt, ...) {
    va_list args;
    int written;

    va_start(args, fmt);
    written = vsnprintf(buffer, size, fmt, args);
    va_end(args);

    return written;
}
