#ifndef OHOS_MEMORY_H
#define OHOS_MEMORY_H

#include "multiboot.h"
#include "types.h"

typedef struct {
    u32 total_bytes;
    u32 heap_start;
    u32 heap_end;
    u32 heap_used;
    u32 heap_free;
} memory_stats_t;

void memory_init(const multiboot_info_t *mbi, uintptr_t kernel_end);
void *kmalloc(size_t size);
void *kcalloc(size_t count, size_t size);
void kfree(void *ptr);
memory_stats_t memory_stats(void);
u32 memory_total_bytes(void);

#endif
