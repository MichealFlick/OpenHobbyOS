#include "memory.h"

#include "console.h"
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

/* Tracing counters */
static u32 merge_call_count;
static u32 merge_blocks_scanned;
static u32 merge_merges_performed;

static u32 align_up(u32 value, u32 alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

static void dump_free_list(const char *label) {
    u32 total_free = 0;
    u32 largest = 0;
    u32 free_count = 0;
    u32 total_blocks = 0;
    block_header_t *block = heap_head;
    while (block) {
        total_blocks++;
        if (block->free) {
            free_count++;
            total_free += block->size;
            if (block->size > largest) largest = block->size;
        }
        block = block->next;
    }
    console_printf("[memtrace] %s: blocks=%u free=%u free_bytes=%u largest=%u\n",
                   label, total_blocks, free_count, total_free, largest);
}

static void merge_blocks(void) {
    block_header_t *block = heap_head;
    merge_call_count++;
    while (block && block->next) {
        merge_blocks_scanned++;
        if (block->free && block->next->free) {
            block->size += sizeof(block_header_t) + block->next->size;
            block->next = block->next->next;
            merge_merges_performed++;
            continue;
        }
        block = block->next;
    }
}

void memory_dump_free(void) {
    dump_free_list("dump");
}

void memory_defragment(void) {
    /* Repeatedly merge all adjacent free blocks to create largest possible contiguous regions */
    bool merged;
    do {
        merged = false;
        block_header_t *block = heap_head;
        while (block && block->next) {
            if (block->free && block->next->free) {
                block->size += sizeof(block_header_t) + block->next->size;
                block->next = block->next->next;
                merged = true;
                continue;
            }
            block = block->next;
        }
    } while (merged);
}

u32 memory_largest_free_block(void) {
    u32 largest = 0;
    block_header_t *block = heap_head;
    while (block) {
        if (block->free && block->size > largest) {
            largest = block->size;
        }
        block = block->next;
    }
    return largest;
}

void memory_init(const multiboot_info_t *mbi, uintptr_t kernel_end) {
    u32 highest_addr = 0;
    u32 mmap_highest = 0;
    u32 mods_start = 0, mods_end = (u32)kernel_end;

    /* Use memory map if available */
    if (mbi->flags & MULTIBOOT_FLAG_MMAP) {
        u32 cursor = mbi->mmap_addr;
        u32 end = mbi->mmap_addr + mbi->mmap_length;
        while (cursor < end) {
            const multiboot_mmap_entry_t *entry = (const multiboot_mmap_entry_t *)(uintptr_t)cursor;
            if (entry->type == 1 && entry->addr_high == 0 && entry->len_high == 0) {
                u32 entry_end = entry->addr_low + entry->len_low;
                if (entry_end > mmap_highest) {
                    mmap_highest = entry_end;
                }
            }
            cursor += entry->size + sizeof(entry->size);
        }
    }

    /* Use mem_upper for total RAM size (more reliable for high memory) */
    if (mbi->flags & MULTIBOOT_FLAG_MEM) {
        highest_addr = 0x100000u + (mbi->mem_upper * 1024u);
    } else {
        highest_addr = mmap_highest;
    }

    /* Find multiboot module range */
    if (mbi->flags & MULTIBOOT_FLAG_MODS) {
        const multiboot_module_t *mods = (const multiboot_module_t *)(uintptr_t)mbi->mods_addr;
        for (u32 i = 0; i < mbi->mods_count; ++i) {
            if (mods[i].mod_start < mods_start || mods_start == 0) {
                mods_start = mods[i].mod_start;
            }
            if (mods[i].mod_end > mods_end) {
                mods_end = mods[i].mod_end;
            }
        }
    }

    if (highest_addr == 0) {
        panic("Unable to discover memory map");
    }

    /* Create two heap regions: before and after modules */
    u32 heap1_start = align_up((u32)kernel_end, 16);
    u32 heap1_end = mods_start > heap1_start ? mods_start : heap1_start;
    u32 heap2_start = align_up(mods_end, 16);
    u32 heap2_end = highest_addr;

    if (heap2_end > USER_BASE - 0x10000u) {
        heap2_end = USER_BASE - 0x10000u;
    }

    /* Initialize first heap region (before modules) if it has space */
    if (heap1_end > heap1_start + sizeof(block_header_t)) {
        heap_head = (block_header_t *)(uintptr_t)heap1_start;
        heap_head->size = heap1_end - heap1_start - sizeof(block_header_t);
        heap_head->free = true;
        heap_start_addr = heap1_start;

        /* Link to second region if it has space */
        if (heap2_end > heap2_start + sizeof(block_header_t)) {
            block_header_t *heap2 = (block_header_t *)(uintptr_t)heap2_start;
            heap2->size = heap2_end - heap2_start - sizeof(block_header_t);
            heap2->free = true;
            heap2->next = NULL;
            heap_head->next = heap2;
            heap_end_addr = heap2_end;
        } else {
            heap_head->next = NULL;
            heap_end_addr = heap1_end;
        }
    } else if (heap2_end > heap2_start + sizeof(block_header_t)) {
        /* Only second region has space */
        heap_head = (block_header_t *)(uintptr_t)heap2_start;
        heap_head->size = heap2_end - heap2_start - sizeof(block_header_t);
        heap_head->free = true;
        heap_head->next = NULL;
        heap_start_addr = heap2_start;
        heap_end_addr = heap2_end;
    } else {
        panic("No room left for kernel heap");
    }

    detected_memory_bytes = highest_addr;

    /* Debug: print heap initialization info */
    u32 total_size = 0;
    block_header_t *block = heap_head;
    while (block) {
        total_size += block->size;
        block = block->next;
    }
    console_printf("[memory] kernel_end=%x, heap_start=%x, heap_end=%x, size=%u KiB\n",
                   (u32)kernel_end, heap_start_addr, heap_end_addr, total_size / 1024);
    dump_free_list("init");
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

/* Aligned allocation - stores original pointer before aligned address */
void *kmalloc_aligned(size_t size, size_t alignment) {
    if (alignment <= 8) {
        return kmalloc(size);
    }
    
    /* Allocate extra space for alignment and storing original pointer */
    size_t total_size = size + alignment + sizeof(void *);
    void *raw = kmalloc(total_size);
    if (!raw) return NULL;
    
    /* Calculate aligned address */
    uintptr_t aligned = ((uintptr_t)raw + sizeof(void *) + alignment - 1) & ~(alignment - 1);
    
    /* Store original pointer before aligned address */
    void **orig_ptr = (void **)(aligned - sizeof(void *));
    *orig_ptr = raw;
    
    return (void *)aligned;
}

void *kcalloc_aligned(size_t count, size_t size, size_t alignment) {
    void *ptr = kmalloc_aligned(count * size, alignment);
    if (ptr) {
        memset(ptr, 0, count * size);
    }
    return ptr;
}

void *krealloc(void *ptr, size_t new_size) {
    if (!ptr) {
        return kmalloc(new_size);
    }

    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }

    block_header_t *block = ((block_header_t *)ptr) - 1;
    size_t old_size = block->size;

    if (block->size >= new_size) {
        return ptr;
    }

    void *new_ptr = kmalloc(new_size);
    if (!new_ptr) {
        return NULL;
    }

    memcpy(new_ptr, ptr, old_size);
    kfree(ptr);

    return new_ptr;
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

u32 memory_heap_end(void) {
    return heap_end_addr;
}
