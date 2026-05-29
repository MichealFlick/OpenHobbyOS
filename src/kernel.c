#include "console.h"
#include "gdt.h"
#include "idt.h"
#include "initrd.h"
#include "io.h"
#include "keyboard.h"
#include "memory.h"
#include "multiboot.h"
#include "netdev.h"
#include "paging.h"
#include "panic.h"
#include "pci.h"
#include "pic.h"
#include "pit.h"
#include "power.h"
#include "rtl8139.h"
#include "task.h"
#include "vfs.h"

static bool kernel_path_is_executable(const char *path) {
    vfs_stat_t st;
    const vfs_node_t *node = vfs_resolve(vfs_root(), path);

    if (!node || !vfs_stat_node(node, &st)) {
        return false;
    }
    return !st.is_dir && ((st.mode & 0111u) != 0);
}

static void start_sdp_compositor(void) {
    memory_stats_t stats_after;

    memory_defragment();
    stats_after = memory_stats();
    console_printf("[init] heap: %u KiB used, %u KiB free, largest=%u KiB\n",
                   stats_after.heap_used / 1024, stats_after.heap_free / 1024,
                   memory_largest_free_block() / 1024);

    console_printf("[init] checking xnx-compositor...\n");
    if (!kernel_path_is_executable("/bin/xnx-compositor")) {
        console_printf("[init] xnx-compositor not executable\n");
        return;
    }
    console_printf("[init] xnx-compositor OK, spawning...\n");

    {
        int pid = task_spawn_background("/bin/xnx-compositor");
        console_printf("[init] XNX compositor spawn: pid=%d\n", pid);
    }

    // if (kernel_path_is_executable("/bin/test_fb")) {
    //     int pid = task_spawn_background("/bin/test_fb");
    //     console_printf("[init] test_fb spawn: pid=%d\n", pid);
    // }

}

extern u8 stack_top;
extern u8 __kernel_end;

static void boot_banner(void) {
    console_write("OpenHobbyOS\n");
    console_write("the small workspace\n\n");
}

void kernel_main(u32 magic, u32 mbi_addr) {
    const multiboot_info_t *mbi = (const multiboot_info_t *)(uintptr_t)mbi_addr;

    console_init();

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        panic("Bootloader did not hand us a multiboot environment");
    }

    console_configure(mbi);

    {
        u32 cr0_val, cr4_val;
        __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0_val));
        __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4_val));
        console_printf("[boot] CR0=%x CR4=%x (PAE=%s)\n",
                       cr0_val, cr4_val,
                       (cr4_val & 0x20) ? "ON" : "OFF");
    }

    gdt_init((uintptr_t)&stack_top);
    pic_remap();
    idt_init();
    pit_init(100);
    keyboard_init();
    memory_init(mbi, (uintptr_t)&__kernel_end);
    paging_init(mbi, memory_total_bytes(), (uintptr_t)&__kernel_end);

    console_activate();
    boot_banner();

    initrd_init(mbi);
    vfs_init();

    pci_scan();
    netdev_init();
    rtl8139_init();

    task_init();
    fpu_init();
    start_sdp_compositor();

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

    console_clear();
    console_printf("[init] spawning GOSH! userspace shell...\n");
    {
        const char *shell_argv[] = {"/bin/gosh", NULL};
        memory_defragment();
        task_run_argv_alongside(NULL, "/bin/gosh", 1, shell_argv);
    }
    console_write("[init] shell exited, halting\n");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
