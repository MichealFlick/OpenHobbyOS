#include "string.h"

void *memcpy(void *dest, const void *src, size_t count) {
    u8 *out = (u8 *)dest;
    const u8 *in = (const u8 *)src;
    while (count--) {
        *out++ = *in++;
    }
    return dest;
}

void *memmove(void *dest, const void *src, size_t count) {
    u8 *out = (u8 *)dest;
    const u8 *in = (const u8 *)src;
    if (out < in) {
        while (count--) {
            *out++ = *in++;
        }
    } else if (out > in) {
        out += count;
        in += count;
        while (count--) {
            *--out = *--in;
        }
    }
    return dest;
}

void *memset(void *dest, int value, size_t count) {
    u8 *out = (u8 *)dest;
    while (count--) {
        *out++ = (u8)value;
    }
    return dest;
}

int memcmp(const void *left, const void *right, size_t count) {
    const u8 *a = (const u8 *)left;
    const u8 *b = (const u8 *)right;
    while (count--) {
        if (*a != *b) {
            return (int)*a - (int)*b;
        }
        a++;
        b++;
    }
    return 0;
}

size_t strlen(const char *value) {
    size_t length = 0;
    while (value[length]) {
        length++;
    }
    return length;
}

int strcmp(const char *left, const char *right) {
    while (*left && *left == *right) {
        left++;
        right++;
    }
    return (unsigned char)*left - (unsigned char)*right;
}

int strncmp(const char *left, const char *right, size_t count) {
    while (count && *left && *left == *right) {
        left++;
        right++;
        count--;
    }
    if (!count) {
        return 0;
    }
    return (unsigned char)*left - (unsigned char)*right;
}

char *strcpy(char *dest, const char *src) {
    char *out = dest;
    while ((*out++ = *src++) != '\0') {
    }
    return dest;
}

char *strncpy(char *dest, const char *src, size_t count) {
    char *out = dest;
    while (count && *src) {
        *out++ = *src++;
        count--;
    }
    while (count--) {
        *out++ = '\0';
    }
    return dest;
}

char *strchr(const char *value, int needle) {
    while (*value) {
        if (*value == (char)needle) {
            return (char *)value;
        }
        value++;
    }
    return needle == '\0' ? (char *)value : NULL;
}

char *strrchr(const char *value, int needle) {
    const char *last = NULL;
    while (*value) {
        if (*value == (char)needle) {
            last = value;
        }
        value++;
    }
    if (needle == '\0') {
        return (char *)value;
    }
    return (char *)last;
}

bool isspace(int ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
}

bool isdigit(int ch) {
    return ch >= '0' && ch <= '9';
}

bool isalpha(int ch) {
    ch = tolower((char)ch);
    return ch >= 'a' && ch <= 'z';
}

char tolower(char ch) {
    return (ch >= 'A' && ch <= 'Z') ? (char)(ch + ('a' - 'A')) : ch;
}

char toupper(char ch) {
    return (ch >= 'a' && ch <= 'z') ? (char)(ch - ('a' - 'A')) : ch;
}
