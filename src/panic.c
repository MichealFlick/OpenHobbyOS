#include "panic.h"

#include "console.h"
#include "io.h"

NORETURN void panic_v(const char *fmt, va_list args) {
    interrupts_disable();
    console_set_color(CONSOLE_WHITE, CONSOLE_RED);
    console_write("\nPANIC: ");
    console_vprintf(fmt, args);
    console_write("\nSystem halted.\n");
    for (;;) {
        cpu_halt();
    }
}

NORETURN void panic(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    panic_v(fmt, args);
    va_end(args);
}
