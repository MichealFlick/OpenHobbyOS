#include "runtime.h"

#include "syscall.h"

static unsigned long long u_divide_u64_u32(unsigned long long dividend, unsigned int divisor) {
    unsigned long long remainder = 0;
    unsigned long long quotient = 0;

    if (divisor == 0) {
        return 0;
    }

    for (int bit = 63; bit >= 0; --bit) {
        remainder = (remainder << 1) | ((dividend >> bit) & 1ull);
        if (remainder >= divisor) {
            remainder -= divisor;
            quotient |= (1ull << bit);
        }
    }

    return quotient;
}

unsigned int u_strlen(const char *text) {
    unsigned int length = 0;
    while (text[length]) {
        length++;
    }
    return length;
}

int u_strcmp(const char *left, const char *right) {
    while (*left && *left == *right) {
        left++;
        right++;
    }
    return (unsigned char)*left - (unsigned char)*right;
}

int u_strncmp(const char *left, const char *right, unsigned int count) {
    unsigned int index = 0;

    while (index < count && left[index] && left[index] == right[index]) {
        index++;
    }

    if (index == count) {
        return 0;
    }
    return (unsigned char)left[index] - (unsigned char)right[index];
}

char *u_strcpy(char *dest, const char *src) {
    char *out = dest;

    while (*src) {
        *out++ = *src++;
    }
    *out = '\0';
    return dest;
}

void *u_memset(void *dest, int value, unsigned int length) {
    unsigned char *out = (unsigned char *)dest;

    for (unsigned int i = 0; i < length; ++i) {
        out[i] = (unsigned char)value;
    }
    return dest;
}

void *u_memcpy(void *dest, const void *src, unsigned int length) {
    unsigned char *out = (unsigned char *)dest;
    const unsigned char *in = (const unsigned char *)src;

    for (unsigned int i = 0; i < length; ++i) {
        out[i] = in[i];
    }
    return dest;
}

void u_puts(const char *text) {
    sys_write(1, text, u_strlen(text));
}

void u_putsn(const char *text, unsigned int length) {
    sys_write(1, text, length);
}

void u_put_uint(unsigned int value) {
    char buffer[16];
    unsigned int used = 0;

    if (value == 0) {
        u_puts("0");
        return;
    }

    while (value) {
        buffer[used++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    while (used) {
        char ch = buffer[--used];
        sys_write(1, &ch, 1);
    }
}

void u_put_hex(unsigned int value) {
    const char *digits = "0123456789abcdef"; // dont ask questions
    char buffer[8];
    unsigned int used = 0;

    if (value == 0) {
        u_puts("0");
        return;
    }

    while (value) {
        buffer[used++] = digits[value & 0xFu];
        value >>= 4;
    }

    while (used) {
        char ch = buffer[--used];
        sys_write(1, &ch, 1);
    }
}

void u_put_int(int value) {
    if (value < 0) {
        u_puts("-");
        u_put_uint((unsigned int)(-value));
        return;
    }

    u_put_uint((unsigned int)value);
}

void u_put_u64(unsigned long long value) {
    char buffer[24];
    unsigned int used = 0;

    if (value == 0) {
        u_puts("0");
        return;
    }

    while (value) {
        unsigned long long next = u_divide_u64_u32(value, 10u);
        unsigned int digit = (unsigned int)(value - next * 10u);
        buffer[used++] = (char)('0' + digit);
        value = next;
    }

    while (used) {
        char ch = buffer[--used];
        sys_write(1, &ch, 1);
    }
}

void u_print_uname(const struct linux_utsname *name) {
    u_puts(name->sysname);
    u_puts(" ");
    u_puts(name->release);
    u_puts(" ");
    u_puts(name->machine);
    u_puts("\n");
    u_puts("node: ");
    u_puts(name->nodename);
    u_puts("\nversion: ");
    u_puts(name->version);
    u_puts("\ndomain: ");
    u_puts(name->domainname);
    u_puts("\n");
}

const char *u_basename(const char *path) {
    const char *tail = path;

    while (*path) {
        if (*path == '/') {
            tail = path + 1;
        }
        path++;
    }

    return tail;
}

void u_print_stat(const char *label, const struct linux_stat64 *stat) {
    u_puts(label);
    u_puts(":\n");
    u_puts("  mode: 0x");
    u_put_hex(stat->st_mode);
    u_puts("\n  size: ");
    u_put_u64((unsigned long long)stat->st_size);
    u_puts("\n  blocks: ");
    u_put_u64((unsigned long long)stat->st_blocks);
    u_puts("\n  inode: ");
    u_put_u64(stat->st_ino);
    u_puts("\n");
}

unsigned int u_parse_uint(const char *text, int *ok) {
    unsigned int value = 0;

    if (ok) {
        *ok = 0;
    }
    if (!text || !*text) {
        return 0;
    }

    while (*text) {
        if (*text < '0' || *text > '9') {
            return 0;
        }
        value = value * 10u + (unsigned int)(*text - '0');
        text++;
    }

    if (ok) {
        *ok = 1;
    }
    return value;
}
