#ifndef OHOS_IO_H
#define OHOS_IO_H

#include "types.h"

static inline void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "d"((u16)port));
}

static inline u8 inb(u16 port) {
    u8 value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "d"((u16)port));
    return value;
}

static inline u16 inw(u16 port) {
    u16 value;
    __asm__ volatile ("inw %1, %0" : "=a"(value) : "d"((u16)port));
    return value;
}

static inline u32 inl(u16 port) {
    u32 value;
    __asm__ volatile ("inl %1, %0" : "=a"(value) : "d"((u16)port));
    return value;
}

static inline void outw(u16 port, u16 value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "d"((u16)port));
}

static inline void outl(u16 port, u32 value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "d"((u16)port));
}

static inline void io_wait(void) {
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}

static inline void interrupts_enable(void) {
    __asm__ volatile ("sti");
}

static inline void interrupts_disable(void) {
    __asm__ volatile ("cli");
}

static inline void cpu_halt(void) {
    __asm__ volatile ("hlt");
}

#endif
