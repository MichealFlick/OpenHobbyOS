#include "pit.h"

#include "idt.h"
#include "io.h"
#include "pic.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_BASE_HZ  1193182u

static volatile u32 ticks_since_boot;
static u32 configured_frequency = 100;

static void pit_irq(UNUSED registers_t *regs) {
    ticks_since_boot++;
}

void pit_init(u32 frequency_hz) {
    u32 divisor;

    if (frequency_hz == 0) {
        frequency_hz = 100;
    }

    configured_frequency = frequency_hz;
    divisor = PIT_BASE_HZ / frequency_hz;

    irq_install_handler(0, pit_irq);
    pic_clear_mask(0);
    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, (u8)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (u8)((divisor >> 8) & 0xFF));
}

u32 pit_ticks(void) {
    return ticks_since_boot;
}

u32 pit_frequency(void) {
    return configured_frequency;
}

void pit_sleep(u32 ticks) {
    u32 goal = pit_ticks() + ticks;
    while ((i32)(goal - pit_ticks()) > 0) {
        cpu_halt();
    }
}
