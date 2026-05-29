#include "idt.h"

#include "console.h"
#include "gdt.h"
#include "paging.h"
#include "panic.h"
#include "pic.h"
#include "string.h"
#include "syscall.h"
#include "task.h"

typedef struct PACKED {
    u16 base_low;
    u16 selector;
    u8 zero;
    u8 flags;
    u16 base_high;
} idt_entry_t;

typedef struct PACKED {
    u16 limit;
    u32 base;
} idt_ptr_t;

static idt_entry_t idt_entries[256];
static idt_ptr_t idt_ptr;
static irq_handler_t irq_handlers[16];
static registers_t last_user_regs;
static registers_t last_user_syscall_regs;
static bool last_user_regs_valid;
static bool last_user_syscall_regs_valid;

extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);
extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);
extern void isr128(void);

static const char *const exception_names[32] = {
    "divide error",
    "debug",
    "non-maskable interrupt",
    "breakpoint",
    "overflow",
    "bound range exceeded",
    "invalid opcode",
    "device not available",
    "double fault",
    "coprocessor segment overrun",
    "invalid TSS",
    "segment not present",
    "stack-segment fault",
    "general protection fault",
    "page fault",
    "reserved",
    "x87 floating point",
    "alignment check",
    "machine check",
    "SIMD floating point",
    "virtualization",
    "control protection",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "hypervisor injection",
    "VMM communication",
    "security",
    "reserved",
};

static void idt_set_gate(u8 index, u32 base, u16 selector, u8 flags) {
    idt_entries[index].base_low = (u16)(base & 0xFFFF);
    idt_entries[index].base_high = (u16)((base >> 16) & 0xFFFF);
    idt_entries[index].selector = selector;
    idt_entries[index].zero = 0;
    idt_entries[index].flags = flags;
}

static void idt_load(const idt_ptr_t *ptr) {
    __asm__ volatile ("lidt (%0)" : : "r"(ptr) : "memory");
}

void idt_init(void) {
    memset(idt_entries, 0, sizeof(idt_entries));
    memset(irq_handlers, 0, sizeof(irq_handlers));
    memset(&last_user_regs, 0, sizeof(last_user_regs));
    memset(&last_user_syscall_regs, 0, sizeof(last_user_syscall_regs));
    last_user_regs_valid = false;
    last_user_syscall_regs_valid = false;

    idt_ptr.limit = sizeof(idt_entries) - 1;
    idt_ptr.base = (u32)(uintptr_t)&idt_entries;

    idt_set_gate(0, (u32)(uintptr_t)isr0, KERNEL_CS, 0x8E);
    idt_set_gate(1, (u32)(uintptr_t)isr1, KERNEL_CS, 0x8E);
    idt_set_gate(2, (u32)(uintptr_t)isr2, KERNEL_CS, 0x8E);
    idt_set_gate(3, (u32)(uintptr_t)isr3, KERNEL_CS, 0x8E);
    idt_set_gate(4, (u32)(uintptr_t)isr4, KERNEL_CS, 0x8E);
    idt_set_gate(5, (u32)(uintptr_t)isr5, KERNEL_CS, 0x8E);
    idt_set_gate(6, (u32)(uintptr_t)isr6, KERNEL_CS, 0x8E);
    idt_set_gate(7, (u32)(uintptr_t)isr7, KERNEL_CS, 0x8E);
    idt_set_gate(8, (u32)(uintptr_t)isr8, KERNEL_CS, 0x8E);
    idt_set_gate(9, (u32)(uintptr_t)isr9, KERNEL_CS, 0x8E);
    idt_set_gate(10, (u32)(uintptr_t)isr10, KERNEL_CS, 0x8E);
    idt_set_gate(11, (u32)(uintptr_t)isr11, KERNEL_CS, 0x8E);
    idt_set_gate(12, (u32)(uintptr_t)isr12, KERNEL_CS, 0x8E);
    idt_set_gate(13, (u32)(uintptr_t)isr13, KERNEL_CS, 0x8E);
    idt_set_gate(14, (u32)(uintptr_t)isr14, KERNEL_CS, 0x8E);
    idt_set_gate(15, (u32)(uintptr_t)isr15, KERNEL_CS, 0x8E);
    idt_set_gate(16, (u32)(uintptr_t)isr16, KERNEL_CS, 0x8E);
    idt_set_gate(17, (u32)(uintptr_t)isr17, KERNEL_CS, 0x8E);
    idt_set_gate(18, (u32)(uintptr_t)isr18, KERNEL_CS, 0x8E);
    idt_set_gate(19, (u32)(uintptr_t)isr19, KERNEL_CS, 0x8E);
    idt_set_gate(20, (u32)(uintptr_t)isr20, KERNEL_CS, 0x8E);
    idt_set_gate(21, (u32)(uintptr_t)isr21, KERNEL_CS, 0x8E);
    idt_set_gate(22, (u32)(uintptr_t)isr22, KERNEL_CS, 0x8E);
    idt_set_gate(23, (u32)(uintptr_t)isr23, KERNEL_CS, 0x8E);
    idt_set_gate(24, (u32)(uintptr_t)isr24, KERNEL_CS, 0x8E);
    idt_set_gate(25, (u32)(uintptr_t)isr25, KERNEL_CS, 0x8E);
    idt_set_gate(26, (u32)(uintptr_t)isr26, KERNEL_CS, 0x8E);
    idt_set_gate(27, (u32)(uintptr_t)isr27, KERNEL_CS, 0x8E);
    idt_set_gate(28, (u32)(uintptr_t)isr28, KERNEL_CS, 0x8E);
    idt_set_gate(29, (u32)(uintptr_t)isr29, KERNEL_CS, 0x8E);
    idt_set_gate(30, (u32)(uintptr_t)isr30, KERNEL_CS, 0x8E);
    idt_set_gate(31, (u32)(uintptr_t)isr31, KERNEL_CS, 0x8E);

    idt_set_gate(32, (u32)(uintptr_t)irq0, KERNEL_CS, 0x8E);
    idt_set_gate(33, (u32)(uintptr_t)irq1, KERNEL_CS, 0x8E);
    idt_set_gate(34, (u32)(uintptr_t)irq2, KERNEL_CS, 0x8E);
    idt_set_gate(35, (u32)(uintptr_t)irq3, KERNEL_CS, 0x8E);
    idt_set_gate(36, (u32)(uintptr_t)irq4, KERNEL_CS, 0x8E);
    idt_set_gate(37, (u32)(uintptr_t)irq5, KERNEL_CS, 0x8E);
    idt_set_gate(38, (u32)(uintptr_t)irq6, KERNEL_CS, 0x8E);
    idt_set_gate(39, (u32)(uintptr_t)irq7, KERNEL_CS, 0x8E);
    idt_set_gate(40, (u32)(uintptr_t)irq8, KERNEL_CS, 0x8E);
    idt_set_gate(41, (u32)(uintptr_t)irq9, KERNEL_CS, 0x8E);
    idt_set_gate(42, (u32)(uintptr_t)irq10, KERNEL_CS, 0x8E);
    idt_set_gate(43, (u32)(uintptr_t)irq11, KERNEL_CS, 0x8E);
    idt_set_gate(44, (u32)(uintptr_t)irq12, KERNEL_CS, 0x8E);
    idt_set_gate(45, (u32)(uintptr_t)irq13, KERNEL_CS, 0x8E);
    idt_set_gate(46, (u32)(uintptr_t)irq14, KERNEL_CS, 0x8E);
    idt_set_gate(47, (u32)(uintptr_t)irq15, KERNEL_CS, 0x8E);

    idt_set_gate(128, (u32)(uintptr_t)isr128, KERNEL_CS, 0xEE);

    idt_load(&idt_ptr);
}

void irq_install_handler(u8 irq, irq_handler_t handler) {
    if (irq < 16) {
        irq_handlers[irq] = handler;
    }
}

void irq_remove_handler(u8 irq) {
    if (irq < 16) {
        irq_handlers[irq] = NULL;
    }
}

bool idt_last_user_frame(registers_t *out) {
    if (!out || !last_user_regs_valid) {
        return false;
    }

    *out = last_user_regs;
    return true;
}

bool idt_last_user_syscall_frame(registers_t *out) {
    if (!out || !last_user_syscall_regs_valid) {
        return false;
    }

    *out = last_user_syscall_regs;
    return true;
}

void isr_dispatch(registers_t *regs) {
    if ((regs->cs & 3u) == 3u) {
        last_user_regs = *regs;
        last_user_regs_valid = true;
        if (regs->int_no == 128) {
            last_user_syscall_regs = *regs;
            last_user_syscall_regs_valid = true;
        }
    }

    /* Handle page fault specially */
    if (regs->int_no == 14) {
        u32 fault_addr;
        __asm__ volatile ("mov %%cr2, %0" : "=r"(fault_addr));
        page_fault_handler(fault_addr, regs->err_code, regs);
        return;
    }

    /* Handle #NM (Device Not Available) for lazy FPU context switching */
    if (regs->int_no == 7) {
        fpu_nm_handler(regs);
        return;
    }

    if (regs->int_no < 32) {
        if ((regs->cs & 3u) == 3u && task_is_active()) {
            task_abort_from_trap(regs, exception_names[regs->int_no]);
        }

        panic("CPU exception %u (%s), err=%x eip=%x",
              regs->int_no,
              exception_names[regs->int_no],
              regs->err_code,
              regs->eip);
    }

    if (regs->int_no >= 32 && regs->int_no < 48) {
        u8 irq = (u8)(regs->int_no - 32);
        if (irq_handlers[irq]) {
            irq_handlers[irq](regs);
        }
        pic_send_eoi(irq);
        return;
    }

    if (regs->int_no == 128) {
        regs->eax = (u32)syscall_dispatch(regs);
        return;
    }

    console_printf("Unhandled interrupt vector %u\n", regs->int_no);
}
