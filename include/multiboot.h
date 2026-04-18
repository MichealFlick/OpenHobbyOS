#ifndef OHOS_MULTIBOOT_H
#define OHOS_MULTIBOOT_H

#include "types.h"

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002
#define MULTIBOOT_FLAG_MEM         (1u << 0)
#define MULTIBOOT_FLAG_MODS        (1u << 3)
#define MULTIBOOT_FLAG_MMAP        (1u << 6)

typedef struct PACKED {
    u32 mod_start;
    u32 mod_end;
    u32 string;
    u32 reserved;
} multiboot_module_t;

typedef struct PACKED {
    u32 size;
    u32 addr_low;
    u32 addr_high;
    u32 len_low;
    u32 len_high;
    u32 type;
} multiboot_mmap_entry_t;

typedef struct PACKED {
    u32 flags;
    u32 mem_lower;
    u32 mem_upper;
    u32 boot_device;
    u32 cmdline;
    u32 mods_count;
    u32 mods_addr;
    u32 syms[4];
    u32 mmap_length;
    u32 mmap_addr;
} multiboot_info_t;

#endif
