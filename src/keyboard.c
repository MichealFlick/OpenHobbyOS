#include "keyboard.h"

#include "console.h"
#include "idt.h"
#include "io.h"
#include "pic.h"
#include "serial.h"
#include "string.h"

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_BUFFER_SIZE 256
#define KEYBOARD_RAW_BUFFER_SIZE 512

static volatile u8 raw_buffer[KEYBOARD_RAW_BUFFER_SIZE];
static volatile u32 raw_head;
static volatile u32 raw_tail;

static const char keymap[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0A] = '9', [0x0B] = '0', [0x0C] = '-', [0x0D] = '=',
    [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
    [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
    [0x18] = 'o', [0x19] = 'p', [0x1A] = '[', [0x1B] = ']',
    [0x1C] = '\n',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f',
    [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
    [0x26] = 'l', [0x27] = ';', [0x28] = '\'',
    [0x29] = '`', [0x2B] = '\\',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v',
    [0x30] = 'b', [0x31] = 'n', [0x32] = 'm', [0x33] = ',',
    [0x34] = '.', [0x35] = '/', [0x37] = '*', [0x39] = ' ',
};

static const char shifted_keymap[128] = {
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$',
    [0x06] = '%', [0x07] = '^', [0x08] = '&', [0x09] = '*',
    [0x0A] = '(', [0x0B] = ')', [0x0C] = '_', [0x0D] = '+',
    [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R',
    [0x14] = 'T', [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I',
    [0x18] = 'O', [0x19] = 'P', [0x1A] = '{', [0x1B] = '}',
    [0x1C] = '\n',
    [0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F',
    [0x22] = 'G', [0x23] = 'H', [0x24] = 'J', [0x25] = 'K',
    [0x26] = 'L', [0x27] = ':', [0x28] = '"',
    [0x29] = '~', [0x2B] = '|',
    [0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V',
    [0x30] = 'B', [0x31] = 'N', [0x32] = 'M', [0x33] = '<',
    [0x34] = '>', [0x35] = '?', [0x37] = '*', [0x39] = ' ',
};

static char input_buffer[KEYBOARD_BUFFER_SIZE];
static volatile u32 input_head;
static volatile u32 input_tail;
static bool shift_down;
static bool caps_lock;
static bool ctrl_down;

static void queue_char(char ch) {
    u32 next = (input_head + 1u) % KEYBOARD_BUFFER_SIZE;
    if (next != input_tail) {
        input_buffer[input_head] = ch;
        input_head = next;
    }
}

static char translate_scancode(u8 scancode) {
    char ch;

    if (scancode >= 128) {
        return 0;
    }

    ch = shift_down ? shifted_keymap[scancode] : keymap[scancode];
    if (isalpha(ch)) {
        bool uppercase = caps_lock ^ shift_down;
        ch = uppercase ? toupper(ch) : tolower(ch);
        if (ctrl_down) {
            return (char)(tolower(ch) - 'a' + 1);
        }
        return ch;
    }
    return ch;
}

static void keyboard_irq(UNUSED registers_t *regs) {
    u8 raw_scancode = inb(KEYBOARD_DATA_PORT);
    u32 raw_next = (raw_head + 1u) % KEYBOARD_RAW_BUFFER_SIZE;
    if (raw_next != raw_tail) {
        raw_buffer[raw_head] = raw_scancode;
        raw_head = raw_next;
    }

    u8 scancode = raw_scancode & 0x7Fu;
    bool released = (raw_scancode & 0x80u) != 0;

    if (scancode == 0x2A || scancode == 0x36) {
        shift_down = !released;
        return;
    }

    if (scancode == 0x1D) {
        ctrl_down = !released;
        return;
    }

    if (released) {
        return;
    }

    if (scancode == 0x3A) {
        caps_lock = !caps_lock;
        return;
    }

    {
        char ch = translate_scancode(scancode);
        if (ch) {
            queue_char(ch);
        }
    }
}

bool keyboard_has_raw_scancode(void) {
    return raw_head != raw_tail;
}

u8 keyboard_read_raw_scancode(void) {
    while (raw_head == raw_tail) {
        cpu_halt();
    }
    u8 sc = raw_buffer[raw_tail];
    raw_tail = (raw_tail + 1u) % KEYBOARD_RAW_BUFFER_SIZE;
    return sc;
}

void keyboard_init(void) {
    input_head = 0;
    input_tail = 0;
    raw_head = 0;
    raw_tail = 0;
    shift_down = false;
    caps_lock = false;
    ctrl_down = false;
    irq_install_handler(1, keyboard_irq);
    pic_clear_mask(1);
}

bool keyboard_has_input(void) {
    return input_head != input_tail || serial_can_read();
}

char keyboard_getchar(void) {
    for (;;) {
        if (input_head != input_tail) {
            char ch = input_buffer[input_tail];
            input_tail = (input_tail + 1u) % KEYBOARD_BUFFER_SIZE;
            return ch;
        }

        if (serial_can_read()) {
            char ch = serial_read_char();
            if (ch == '\r') {
                return '\n';
            }
            if (ch == 0x7F) {
                return '\b';
            }
            return ch;
        }

        cpu_halt();
    }
}

size_t keyboard_readline(char *buffer, size_t size) {
    size_t used = 0;

    if (size == 0) {
        return 0;
    }

    for (;;) {
        char ch = keyboard_getchar();

        if (ch == '\b') {
            if (used) {
                used--;
                console_putc('\b');
            }
            continue;
        }

        if (ch == '\n') {
            console_putc('\n');
            break;
        }

        if (ch == 3) {
            console_write("^C\n");
            used = 0;
            break;
        }

        if (used + 1 < size) {
            buffer[used++] = ch;
            console_putc(ch);
        }
    }

    buffer[used] = '\0';
    return used;
}
