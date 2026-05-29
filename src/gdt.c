#include "gdt.h"

#include "string.h"

typedef struct PACKED {
    u16 limit_low;
    u16 base_low;
    u8 base_mid;
    u8 access;
    u8 granularity;
    u8 base_high;
} gdt_entry_t;

typedef struct PACKED {
    u16 limit;
    u32 base;
} gdt_ptr_t;

typedef struct PACKED {
    u32 prev_tss;
    u32 esp0;
    u32 ss0;
    u32 esp1;
    u32 ss1;
    u32 esp2;
    u32 ss2;
    u32 cr3;
    u32 eip;
    u32 eflags;
    u32 eax;
    u32 ecx;
    u32 edx;
    u32 ebx;
    u32 esp;
    u32 ebp;
    u32 esi;
    u32 edi;
    u32 es;
    u32 cs;
    u32 ss;
    u32 ds;
    u32 fs;
    u32 gs;
    u32 ldt;
    u16 trap;
    u16 iomap_base;
} tss_entry_t;

static gdt_entry_t gdt_entries[8];
static gdt_ptr_t gdt_ptr;
static tss_entry_t tss;

static void gdt_set_gate(int index, u32 base, u32 limit, u8 access, u8 granularity) {
    gdt_entries[index].base_low = (u16)(base & 0xFFFF);
    gdt_entries[index].base_mid = (u8)((base >> 16) & 0xFF);
    gdt_entries[index].base_high = (u8)((base >> 24) & 0xFF);
    gdt_entries[index].limit_low = (u16)(limit & 0xFFFF);
    gdt_entries[index].granularity = (u8)(((limit >> 16) & 0x0F) | (granularity & 0xF0));
    gdt_entries[index].access = access;
}

static void tss_write(uintptr_t kernel_stack_top) {
    u32 base = (u32)(uintptr_t)&tss;
    u32 limit = sizeof(tss_entry_t) - 1;

    memset(&tss, 0, sizeof(tss));
    tss.ss0 = KERNEL_DS;
    tss.esp0 = (u32)kernel_stack_top;
    tss.cs = USER_CS;
    tss.ss = USER_DS;
    tss.ds = USER_DS;
    tss.es = USER_DS;
    tss.fs = USER_DS;
    tss.gs = USER_DS;
    tss.iomap_base = sizeof(tss_entry_t);

    gdt_set_gate(5, base, limit, 0xE9, 0x00);
}

static void gdt_load(const gdt_ptr_t *ptr) {
    __asm__ volatile (
        "lgdt (%0)\n"
        "movw %1, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        "movw %%ax, %%ss\n"
        "pushl %2\n"
        "pushl $1f\n"
        "lret\n"
        "1:\n"
        :
        : "r"(ptr), "i"(KERNEL_DS), "i"(KERNEL_CS)
        : "ax", "memory"
    );
}

static void tss_load(void) {
    __asm__ volatile (
        "movw $0x28, %%ax\n"
        "ltr %%ax\n"
        :
        :
        : "ax", "memory"
    );
}

void gdt_init(uintptr_t kernel_stack_top) {
    gdt_ptr.limit = sizeof(gdt_entries) - 1;
    gdt_ptr.base = (u32)(uintptr_t)&gdt_entries;

    gdt_set_gate(0, 0, 0, 0, 0);
    gdt_set_gate(1, 0, 0xFFFFF, 0x9A, 0xCF);
    gdt_set_gate(2, 0, 0xFFFFF, 0x92, 0xCF);
    gdt_set_gate(3, 0, 0xFFFFF, 0xFA, 0xCF);
    gdt_set_gate(4, 0, 0xFFFFF, 0xF2, 0xCF);
    /* Entry 6: per-task TLS/GS segment (selector 0x33). Base updated at task switch */
    gdt_set_gate(6, 0, 0xFFF, 0xF2, 0x40);
    tss_write(kernel_stack_top);

    gdt_load(&gdt_ptr);
    tss_load();
}

void gdt_set_kernel_stack(uintptr_t kernel_stack_top) {
    tss.esp0 = (u32)kernel_stack_top;
}

void gdt_set_gs_base(u32 base) {
    gdt_set_gate(6, base, 0xFFF, 0xF2, 0x40);
}
