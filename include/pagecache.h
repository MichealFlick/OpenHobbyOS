#ifndef OHOS_PAGECACHE_H
#define OHOS_PAGECACHE_H

#include "types.h"
#include "blkdev.h"

/* Page cache entry */
typedef struct page_cache_entry {
    u32 block_num;              /* Block number on device */
    u8 *data;                   /* Cached data (PAGE_SIZE bytes) */
    struct page_cache_entry *lru_next;
    struct page_cache_entry *lru_prev;
    u32 ref_count;              /* Reference count */
    bool dirty;                 /* Needs writeback */
    bool valid;                 /* Entry is valid */
} page_cache_entry_t;

/* Page cache statistics */
typedef struct {
    u32 hits;                   /* Cache hits */
    u32 misses;                 /* Cache misses */
    u32 evictions;              /* Evicted entries */
    u32 writebacks;             /* Dirty pages written back */
} page_cache_stats_t;

/* Page cache API */
bool page_cache_init(u32 max_entries);

/* Read/write through cache */
ssize_t page_cache_read(u32 block_num, void *buffer, size_t offset, size_t count);
ssize_t page_cache_write(u32 block_num, const void *buffer, size_t offset, size_t count);

/* Cache management */
void page_cache_flush(void);        /* Write all dirty pages */
void page_cache_flush_block(u32 block_num);
void page_cache_invalidate(u32 block_num);
void page_cache_invalidate_all(void);

/* Get statistics */
page_cache_stats_t page_cache_get_stats(void);

/* Configuration */
#define PAGECACHE_DEFAULT_ENTRIES   256     /* Default cache size (1MB) */
#define PAGECACHE_MAX_ENTRIES       1024    /* Max cache size (4MB) */

#endif
