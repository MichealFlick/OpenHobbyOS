#ifndef OHOS_INITRD_H
#define OHOS_INITRD_H

#include "multiboot.h"
#include "types.h"

typedef struct {
    const char *name;
    const u8 *data;
    u32 size;
    u32 flags;
    u32 index;
} initrd_file_t;

void initrd_init(const multiboot_info_t *mbi);
bool initrd_ready(void);
u32 initrd_count(void);
const initrd_file_t *initrd_file_at(u32 index);
const initrd_file_t *initrd_find(const char *path);
u32 initrd_module_size(void);

#endif
