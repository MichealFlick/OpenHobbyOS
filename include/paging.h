#ifndef OHOS_PAGING_H
#define OHOS_PAGING_H

#include "idt.h"
#include "multiboot.h"
#include "types.h"

/* x86 paging constants */
#define PAGE_SIZE       4096
#define PAGE_MASK       0xFFFFF000
#define PAGE_OFFSET     0xFFF
#define ENTRIES_PER_PT  1024
#define ENTRIES_PER_PD  1024

/* Page directory/table entry flags */
#define PTE_PRESENT     0x001   /* Page is present in memory */
#define PTE_RW          0x002   /* Read/write permission */
#define PTE_USER        0x004   /* User-accessible (else kernel-only) */
#define PTE_WRITETHRU   0x008   /* Write-through caching */
#define PTE_NOCACHE     0x010   /* Disable caching */
#define PTE_ACCESSED    0x020   /* CPU set this when page is accessed */
#define PTE_DIRTY       0x040   /* CPU set this when page is written */
#define PTE_PAT         0x080   /* Page attribute table */
#define PTE_GLOBAL      0x100   /* Global page (not flushed from TLB) */
#define PTE_COW         0x200   /* Copy-on-write (software-defined) */
#define PTE_ALLOCATED   0x400   /* Frame was allocated by us (software-defined) */
#define PTE_DEMAND_ZERO 0x800   /* Demand-zero page (allocate on first fault) */

/* Page backing types for demand paging */
typedef enum {
    PAGE_BACKING_ANON = 0,  /* Anonymous (zero-filled) */
    PAGE_BACKING_FILE,      /* File-backed (ELF, mmap) */
} page_backing_type_t;

/* Demand paging info - stored per page table entry (swapped with phys addr when mapped) */
typedef struct {
    page_backing_type_t type;
    u32 file_offset;        /* Offset in file (for file-backed) */
    u32 file_size;          /* Size of data in file (may be < PAGE_SIZE for BSS) */
} page_demand_info_t;

/* Address space layout - matches task.h */
#define KERNEL_VIRTUAL_BASE     0xC0000000  /* 3GB - kernel starts here */
#define KERNEL_PD_INDEX         (KERNEL_VIRTUAL_BASE >> 22)

/* Physical memory layout (before mapping) */
#define PHYS_KERNEL_START       0x00100000  /* 1MB */
#define PHYS_HEAP_START         0x01000000  /* 16MB - kernel heap */
#define PHYS_USER_PAGES_START   0x10000000  /* 256MB - user page frames */

/* Page directory/table types */
typedef u32 page_table_entry_t;
typedef u32 page_directory_entry_t;

/* Page directory structure - 4KB aligned */
typedef struct {
    page_directory_entry_t entries[ENTRIES_PER_PD];
} page_directory_t;

/* Page table structure - 4KB aligned */
typedef struct {
    page_table_entry_t entries[ENTRIES_PER_PT];
} page_table_t;

/* Physical frame allocator state */
typedef struct {
    u32 *bitmap;
    u32 total_frames;
    u32 used_frames;
    u32 first_frame;
} frame_allocator_t;

/* Page fault error code flags */
#define PF_ERR_PRESENT      0x01    /* Page was present */
#define PF_ERR_RW           0x02    /* Write access */
#define PF_ERR_USER         0x04    /* User mode */
#define PF_ERR_RESERVED     0x08    /* Reserved bit set */
#define PF_ERR_FETCH        0x10    /* Instruction fetch */

/* Initialize paging subsystem */
void paging_init(const multiboot_info_t *mbi, u32 total_memory_bytes, u32 kernel_end_phys);

/* Physical frame allocation */
bool frame_alloc_init(u32 memory_start, u32 memory_end, u32 first_frame);
u32 frame_alloc(void);
void frame_free(u32 frame);
u32 frame_total(void);
u32 frame_used(void);
u32 frame_free_count(void);

/* Page directory management */
page_directory_t *page_directory_create(void);
void page_directory_destroy(page_directory_t *pd);
void page_directory_switch(page_directory_t *pd);
page_directory_t *page_directory_get_current(void);
page_directory_t *page_directory_get_kernel(void);
void page_directory_switch_to_kernel(void);

/* Page mapping operations */
bool page_map(page_directory_t *pd, u32 virt_addr, u32 phys_addr, u16 flags);
bool page_map_existing(page_directory_t *pd, u32 virt_addr, u32 phys_addr, u16 flags);
bool page_unmap(page_directory_t *pd, u32 virt_addr);
u32 page_get_phys(page_directory_t *pd, u32 virt_addr);
bool page_is_present(page_directory_t *pd, u32 virt_addr);
bool page_set_flags(page_directory_t *pd, u32 virt_addr, u16 flags);

/* Kernel page directory operations */
void paging_map_kernel(page_directory_t *pd);
bool paging_map_kernel_range(page_directory_t *pd, u32 virt_start, u32 phys_start, size_t size, u16 flags);

/* User page operations */
bool paging_alloc_user_page(page_directory_t *pd, u32 virt_addr, u16 flags);
bool paging_free_user_page(page_directory_t *pd, u32 virt_addr);
bool paging_copy_page(page_directory_t *src_pd, page_directory_t *dst_pd, u32 virt_addr, bool cow);

/* Demand paging */
bool paging_setup_demand_anon(page_directory_t *pd, u32 virt_addr, u16 flags);

/* Memory pressure handling */
bool paging_memory_pressure(void);  /* Returns true if low on memory */
u32 paging_available_memory(void);  /* Returns available memory in KB */
void paging_evict_pages(u32 target_kb); /* Evict pages to free memory */

/* Page fault handler */
void page_fault_handler(u32 virt_addr, u32 error_code, registers_t *regs);

/* TLB operations */
static inline void paging_invalidate_tlb(u32 virt_addr) {
    __asm__ volatile ("invlpg (%0)" : : "r"(virt_addr) : "memory");
}

static inline void paging_flush_tlb(void) {
    u32 cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile ("mov %0, %%cr3" : : "r"(cr3));
}

/* Get/set CR3 */
static inline void paging_set_cr3(u32 pd_phys) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(pd_phys) : "memory");
}

static inline u32 paging_get_cr3(void) {
    u32 cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

/* Enable paging in CR0 */
static inline void paging_enable(void) {
    __asm__ volatile (
        "mov %%cr0, %%eax\n"
        "or $0x80000000, %%eax\n"
        "mov %%eax, %%cr0\n"
        ::: "eax", "memory"
    );
}

/* Disable paging in CR0 */
static inline void paging_disable(void) {
    __asm__ volatile (
        "mov %%cr0, %%eax\n"
        "and $0x7FFFFFFF, %%eax\n"
        "mov %%eax, %%cr0\n"
        ::: "eax", "memory"
    );
}

/* Page entry manipulation - exposed for use by task.c for fork/clone */
static inline u32 entry_get_phys(u32 entry) {
    return entry & PAGE_MASK;
}

static inline u32 entry_get_flags(u32 entry) {
    return entry & PAGE_OFFSET;
}

static inline u32 entry_create(u32 phys, u16 flags) {
    return (phys & PAGE_MASK) | (flags & PAGE_OFFSET);
}

/* Address translation helpers */
static inline u32 virt_to_pd_index(u32 virt_addr) {
    return virt_addr >> 22;
}

static inline u32 virt_to_pt_index(u32 virt_addr) {
    return (virt_addr >> 12) & 0x3FF;
}

static inline u32 page_align(u32 addr) {
    return addr & PAGE_MASK;
}

static inline u32 page_offset(u32 addr) {
    return addr & PAGE_OFFSET;
}

static inline bool is_page_aligned(u32 addr) {
    return (addr & PAGE_OFFSET) == 0;
}

#endif
