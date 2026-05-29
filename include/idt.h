#ifndef OHOS_IDT_H
#define OHOS_IDT_H

#include "types.h"

typedef struct PACKED {
    u32 edi;
    u32 esi;
    u32 ebp;
    u32 esp;
    u32 ebx;
    u32 edx;
    u32 ecx;
    u32 eax;
    u32 gs;
    u32 fs;
    u32 es;
    u32 ds;
    u32 int_no;
    u32 err_code;
    u32 eip;
    u32 cs;
    u32 eflags;
    u32 useresp;
    u32 ss;
} registers_t;

typedef void (*irq_handler_t)(registers_t *regs);

void idt_init(void);
void irq_install_handler(u8 irq, irq_handler_t handler);
void irq_remove_handler(u8 irq);
bool idt_last_user_frame(registers_t *out);
bool idt_last_user_syscall_frame(registers_t *out);

#endif
