#ifndef OHOS_MULTIBOOT_H
#define OHOS_MULTIBOOT_H

#include "types.h"

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002
#define MULTIBOOT_FLAG_MEM         (1u << 0)
#define MULTIBOOT_FLAG_MODS        (1u << 3)
#define MULTIBOOT_FLAG_MMAP        (1u << 6)
#define MULTIBOOT_FLAG_VBE         (1u << 11)
#define MULTIBOOT_FLAG_FRAMEBUFFER (1u << 12)

#define MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED 0
#define MULTIBOOT_FRAMEBUFFER_TYPE_RGB     1
#define MULTIBOOT_FRAMEBUFFER_TYPE_TEXT    2

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
    u32 addr;
    u16 num_colors;
} multiboot_framebuffer_palette_t;

typedef struct PACKED {
    u8 red_field_position;
    u8 red_mask_size;
    u8 green_field_position;
    u8 green_mask_size;
    u8 blue_field_position;
    u8 blue_mask_size;
} multiboot_framebuffer_rgb_t;

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
    u32 drives_length;
    u32 drives_addr;
    u32 config_table;
    u32 boot_loader_name;
    u32 apm_table;
    u32 vbe_control_info;
    u32 vbe_mode_info;
    u16 vbe_mode;
    u16 vbe_interface_seg;
    u16 vbe_interface_off;
    u16 vbe_interface_len;
    u64 framebuffer_addr;
    u32 framebuffer_pitch;
    u32 framebuffer_width;
    u32 framebuffer_height;
    u8 framebuffer_bpp;
    u8 framebuffer_type;
    union PACKED {
        multiboot_framebuffer_palette_t palette;
        multiboot_framebuffer_rgb_t rgb;
    } framebuffer_color_info;
} multiboot_info_t;

#endif
