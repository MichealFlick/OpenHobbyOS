#include "memory.h"

#include "panic.h"
#include "string.h"
#include "task.h"

typedef struct block_header {
    u32 size;
    bool free;
    struct block_header *next;
} block_header_t;

static block_header_t *heap_head;
static u32 detected_memory_bytes;
static u32 heap_start_addr;
static u32 heap_end_addr;

static u32 align_up(u32 value, u32 alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

static void merge_blocks(void) {
    block_header_t *block = heap_head;
    while (block && block->next) {
        if (block->free && block->next->free) {
            block->size += sizeof(block_header_t) + block->next->size;
            block->next = block->next->next;
            continue;
        }
        block = block->next;
    }
}

void memory_init(const multiboot_info_t *mbi, uintptr_t kernel_end) {
    u32 highest_addr = 0;
    u32 reserved_end = (u32)kernel_end;

    if (mbi->flags & MULTIBOOT_FLAG_MMAP) {
        u32 cursor = mbi->mmap_addr;
        u32 end = mbi->mmap_addr + mbi->mmap_length;
        while (cursor < end) {
            const multiboot_mmap_entry_t *entry = (const multiboot_mmap_entry_t *)(uintptr_t)cursor;
            if (entry->type == 1 && entry->addr_high == 0 && entry->len_high == 0) {
                u32 entry_end = entry->addr_low + entry->len_low;
                if (entry_end > highest_addr) {
                    highest_addr = entry_end;
                }
            }
            cursor += entry->size + sizeof(entry->size);
        }
    } else if (mbi->flags & MULTIBOOT_FLAG_MEM) {
        highest_addr = 0x100000u + (mbi->mem_upper * 1024u);
    }

    if (mbi->flags & MULTIBOOT_FLAG_MODS) {
        const multiboot_module_t *mods = (const multiboot_module_t *)(uintptr_t)mbi->mods_addr;
        for (u32 i = 0; i < mbi->mods_count; ++i) {
            if (mods[i].mod_end > reserved_end) {
                reserved_end = mods[i].mod_end;
            }
        }
    }

    if (highest_addr == 0) {
        panic("Unable to discover memory map");
    }

    heap_start_addr = align_up(reserved_end, 16);
    heap_end_addr = highest_addr;

    if (heap_end_addr > USER_BASE - 0x10000u) {
        heap_end_addr = USER_BASE - 0x10000u;
    }

    if (heap_end_addr <= heap_start_addr + sizeof(block_header_t)) {
        panic("No room left for kernel heap");
    }

    heap_head = (block_header_t *)(uintptr_t)heap_start_addr;
    heap_head->size = heap_end_addr - heap_start_addr - sizeof(block_header_t);
    heap_head->free = true;
    heap_head->next = NULL;

    detected_memory_bytes = highest_addr;
}

void *kmalloc(size_t size) {
    block_header_t *block;
    size_t needed;

    if (size == 0) {
        return NULL;
    }

    needed = align_up((u32)size, 8);
    block = heap_head;

    while (block) {
        if (block->free && block->size >= needed) {
            if (block->size > needed + sizeof(block_header_t) + 8) {
                block_header_t *split = (block_header_t *)((u8 *)(block + 1) + needed);
                split->size = block->size - needed - sizeof(block_header_t);
                split->free = true;
                split->next = block->next;
                block->next = split;
                block->size = needed;
            }

            block->free = false;
            return block + 1;
        }
        block = block->next;
    }

    return NULL;
}

void *kcalloc(size_t count, size_t size) {
    size_t total = count * size;
    void *ptr = kmalloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void kfree(void *ptr) {
    block_header_t *block;

    if (!ptr) {
        return;
    }

    block = ((block_header_t *)ptr) - 1;
    block->free = true;
    merge_blocks();
}

memory_stats_t memory_stats(void) {
    memory_stats_t stats;
    block_header_t *block = heap_head;

    stats.total_bytes = detected_memory_bytes;
    stats.heap_start = heap_start_addr;
    stats.heap_end = heap_end_addr;
    stats.heap_used = 0;
    stats.heap_free = 0;

    while (block) {
        if (block->free) {
            stats.heap_free += block->size;
        } else {
            stats.heap_used += block->size;
        }
        block = block->next;
    }

    return stats;
}

u32 memory_total_bytes(void) {
    return detected_memory_bytes;
}
