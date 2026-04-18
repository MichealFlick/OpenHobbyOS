#include "initrd.h"

#include "memory.h"
#include "panic.h"
#include "string.h"

typedef struct PACKED {
    char magic[8];
    u32 version;
    u32 count;
    u32 data_offset;
} initrd_header_t;

typedef struct PACKED {
    char name[64];
    u32 offset;
    u32 size;
    u32 flags;
} initrd_entry_t;

static bool mounted;
static u32 file_count;
static u32 module_size;
static initrd_file_t *files;

void initrd_init(const multiboot_info_t *mbi) {
    const initrd_header_t *header;
    const initrd_entry_t *entries;
    const multiboot_module_t *mods;
    const u8 *base;

    mounted = false;
    file_count = 0;
    module_size = 0;
    files = NULL;

    if (!(mbi->flags & MULTIBOOT_FLAG_MODS) || mbi->mods_count == 0) {
        return;
    }

    mods = (const multiboot_module_t *)(uintptr_t)mbi->mods_addr;
    base = (const u8 *)(uintptr_t)mods[0].mod_start;
    module_size = mods[0].mod_end - mods[0].mod_start;

    if (module_size < sizeof(initrd_header_t)) {
        panic("Initrd is too small");
    }

    header = (const initrd_header_t *)base;
    if (memcmp(header->magic, "OHOSRD1\0", 8) != 0 || header->version != 1) {
        panic("Initrd header is invalid");
    }

    if (module_size < sizeof(initrd_header_t) + header->count * sizeof(initrd_entry_t)) {
        panic("Initrd directory is truncated");
    }

    files = (initrd_file_t *)kcalloc(header->count, sizeof(initrd_file_t));
    if (!files) {
        panic("Out of memory while mounting initrd");
    }

    entries = (const initrd_entry_t *)(base + sizeof(initrd_header_t));
    for (u32 i = 0; i < header->count; ++i) {
        if (entries[i].offset + entries[i].size > module_size) {
            panic("Initrd file escapes archive");
        }
        files[i].name = entries[i].name;
        files[i].data = base + entries[i].offset;
        files[i].size = entries[i].size;
        files[i].flags = entries[i].flags;
        files[i].index = i;
    }

    file_count = header->count;
    mounted = true;
}

bool initrd_ready(void) {
    return mounted;
}

u32 initrd_count(void) {
    return file_count;
}

const initrd_file_t *initrd_file_at(u32 index) {
    if (!mounted || index >= file_count) {
        return NULL;
    }
    return &files[index];
}

const initrd_file_t *initrd_find(const char *path) {
    if (!mounted) {
        return NULL;
    }
    for (u32 i = 0; i < file_count; ++i) {
        if (strcmp(files[i].name, path) == 0) {
            return &files[i];
        }
    }
    return NULL;
}

u32 initrd_module_size(void) {
    return module_size;
}
