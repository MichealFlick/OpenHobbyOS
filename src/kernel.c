#include "console.h"
#include "gdt.h"
#include "idt.h"
#include "initrd.h"
#include "io.h"
#include "keyboard.h"
#include "memory.h"
#include "multiboot.h"
#include "panic.h"
#include "pic.h"
#include "pit.h"
#include "power.h"
#include "shell.h"
#include "task.h"
#include "vfs.h"

extern u8 stack_top;
extern u8 __kernel_end;

static void boot_banner(void) {
    console_set_color(CONSOLE_LIGHT_GREEN, CONSOLE_BLACK);
    console_write("OpenHobbyOS\n");
    console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
    console_write("the small workspace\n\n");
}

void kernel_main(u32 magic, u32 mbi_addr) {
    const multiboot_info_t *mbi = (const multiboot_info_t *)(uintptr_t)mbi_addr;

    console_init();
    boot_banner();

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        panic("Bootloader did not hand us a multiboot environment");
    }

    gdt_init((uintptr_t)&stack_top);
    pic_remap();
    idt_init();
    pit_init(100);
    keyboard_init();
    memory_init(mbi, (uintptr_t)&__kernel_end);
    initrd_init(mbi);
    vfs_init();
    task_init();
    power_init();

    interrupts_enable();

    console_printf("heap ready, initrd files: %u, total memory: %u KiB\n",
                   initrd_count(),
                   memory_total_bytes() / 1024u);
    if (task_can_run()) {
        console_write("user ABI path is live\n");
    } else {
        console_write("user ABI path is offline: not enough memory or no initrd\n");
    }
    {
        power_info_t info = power_info();
        if (info.can_shutdown && info.can_suspend && !info.suspend_uses_fallback && !info.used_emulator_fallbacks) {
            console_write("power path is live: shutdown, reboot, and suspend are ACPI-backed\n");
        } else if (info.can_shutdown && info.can_suspend && info.suspend_uses_fallback) {
            console_write("power path is live: shutdown/reboot are ready, suspend falls back to idle wait here\n");
        } else if (info.can_shutdown) {
            console_write("power path is live enough: shutdown and reboot are ready\n");
        } else {
            console_write("power path is thin: reboot is ready, shutdown needs emulator fallback\n");
        }
    }
    console_putc('\n');

    shell_run();
}
