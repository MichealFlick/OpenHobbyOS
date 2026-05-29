#include "paging.h"

#include "console.h"
#include "memory.h"
#include "panic.h"
#include "string.h"
#include "task.h"

/* Frame allocator state */
static frame_allocator_t g_frame_allocator;
static page_directory_t *kernel_page_directory = NULL;
static u32 kernel_page_directory_phys = 0;
static bool paging_active = false;

/* Convert virtual kernel address to physical (assumes identity-mapped during early boot) */
static inline u32 virt_to_phys_early(void *virt) {
    return (u32)(uintptr_t)virt;
}

/* Return a pointer to a page table at the given physical address.
 * Before paging is enabled (paging_init) the CPU uses physical addresses
 * directly, so we must use identity-mapped pointers.  After paging is
 * enabled we rely on the kernel's higher-half mapping
 * (KERNEL_VIRTUAL_BASE → phys 0 for 1 GB) which is present in every
 * page directory via the copied kernel PDEs at indices 768–1023. */
static inline page_table_t *ptable_ptr(u32 pt_phys) {
    if (paging_active) {
        return (page_table_t *)(KERNEL_VIRTUAL_BASE + pt_phys);
    }
    return (page_table_t *)(uintptr_t)pt_phys;
}

/*
 * Physical Frame Allocator
 * Uses bitmap to track free/used frames
 */

bool frame_alloc_init(u32 memory_start, u32 memory_end, u32 first_frame) {
    u32 total_memory = memory_end - memory_start;
    u32 num_frames = total_memory / PAGE_SIZE;
    u32 bitmap_size = ((num_frames + 31) / 32) * sizeof(u32);
    
    /* Allocate bitmap from kernel heap */
    g_frame_allocator.bitmap = kmalloc(bitmap_size);
    if (!g_frame_allocator.bitmap) {
        return false;
    }
    
    memset(g_frame_allocator.bitmap, 0, bitmap_size);
    g_frame_allocator.total_frames = num_frames;
    g_frame_allocator.used_frames = 0;
    g_frame_allocator.first_frame = first_frame;
    
    /* Mark frames before first_frame as used */
    for (u32 i = 0; i < first_frame; i++) {
        u32 idx = i / 32;
        u32 bit = i % 32;
        g_frame_allocator.bitmap[idx] |= (1U << bit);
        g_frame_allocator.used_frames++;
    }
    
    console_printf("[paging] frame allocator: %u frames (%u MiB), bitmap at %p\n",
                   num_frames, total_memory / (1024 * 1024), g_frame_allocator.bitmap);
    
    return true;
}

u32 frame_alloc(void) {
    for (u32 i = 0; i < g_frame_allocator.total_frames; i++) {
        u32 idx = i / 32;
        u32 bit = i % 32;
        
        if (!(g_frame_allocator.bitmap[idx] & (1U << bit))) {
            g_frame_allocator.bitmap[idx] |= (1U << bit);
            g_frame_allocator.used_frames++;
            return (g_frame_allocator.first_frame + i) * PAGE_SIZE;
        }
    }
    return 0; /* Out of memory */
}

void frame_free(u32 phys_addr) {
    if (phys_addr == 0) return;
    
    u32 frame = phys_addr / PAGE_SIZE;
    if (frame < g_frame_allocator.first_frame || frame >= g_frame_allocator.first_frame + g_frame_allocator.total_frames) {
        return;
    }
    
    u32 i = frame - g_frame_allocator.first_frame;
    u32 idx = i / 32;
    u32 bit = i % 32;
    
    g_frame_allocator.bitmap[idx] &= ~(1U << bit);
    g_frame_allocator.used_frames--;
}

u32 frame_total(void) {
    return g_frame_allocator.total_frames;
}

u32 frame_used(void) {
    return g_frame_allocator.used_frames;
}

u32 frame_free_count(void) {
    return g_frame_allocator.total_frames - g_frame_allocator.used_frames;
}

/*
 * Page Directory/Table Management
 */

page_directory_t *page_directory_create(void) {
    u32 pd_phys = frame_alloc();
    if (!pd_phys) return NULL;

    page_directory_t *pd = (page_directory_t *)(KERNEL_VIRTUAL_BASE + pd_phys);
    memset(pd, 0, sizeof(page_directory_t));

    if (kernel_page_directory) {
        /*
         * Copy all kernel identity-mapped PDEs except those in the user
         * address range (32–80 MB, PDE indices 8–19).  PDEs 0–7 cover
         * kernel data structures (code, heap, kmalloc'd page directories).
         * PDEs 20+ cover any remaining identity mappings (e.g. a
         * framebuffer at a high physical address).  Skipping only the
         * user-range PDEs forces page_map to allocate fresh page tables
         * with the correct user-bit (PTE_USER) set when user pages are
         * mapped, eliminating the ghost supervisor-only PTEs that cause
         * protection violations.
         * Clear PTE_ALLOCATED so page_directory_destroy knows these page
         * tables belong to the kernel.
         */
        for (u32 i = 0; i < KERNEL_PD_INDEX; i++) {
            if (i >= (USER_BASE >> 22) && i < (USER_LIMIT >> 22)) {
                continue;
            }
            if (kernel_page_directory->entries[i] & PTE_PRESENT) {
                pd->entries[i] = kernel_page_directory->entries[i] & ~PTE_ALLOCATED;
            }
        }

        /* Copy kernel higher-half mappings (768-1023 = 3GB-4GB) */
        for (int i = KERNEL_PD_INDEX; i < ENTRIES_PER_PD; i++) {
            pd->entries[i] = kernel_page_directory->entries[i];
        }
    }

    return pd;
}

void page_directory_destroy(page_directory_t *pd) {
    if (!pd) return;
    
    /* Free user page tables (those allocated by this process, not shared kernel identity) */
    for (u32 i = 0; i < KERNEL_PD_INDEX; i++) {
        if (!(pd->entries[i] & PTE_PRESENT)) {
            continue;
        }
        
        /* Skip kernel-owned PDEs (identity-mapped, copied without PTE_ALLOCATED) */
        if (!(pd->entries[i] & PTE_ALLOCATED)) {
            continue;
        }
        
        u32 pt_phys = entry_get_phys(pd->entries[i]);
        
        /* Free all frames in this page table */
        page_table_t *pt = ptable_ptr( pt_phys);
        for (int j = 0; j < ENTRIES_PER_PT; j++) {
            if (pt->entries[j] & PTE_PRESENT) {
                u32 flags = entry_get_flags(pt->entries[j]);
                if (flags & PTE_ALLOCATED) {
                    frame_free(entry_get_phys(pt->entries[j]));
                }
            }
        }
        
        /* Free the page table itself */
        if (pd->entries[i] & PTE_ALLOCATED) {
            frame_free(pt_phys);
        }
    }
    
    u32 pd_phys = (u32)(uintptr_t)pd;
    if (pd_phys >= KERNEL_VIRTUAL_BASE) {
        pd_phys -= KERNEL_VIRTUAL_BASE;
    }
    frame_free(pd_phys);
}

void page_directory_switch(page_directory_t *pd) {
    if (!pd) return;
    u32 phys_addr = (u32)(uintptr_t)pd;
    if (phys_addr >= KERNEL_VIRTUAL_BASE) {
        phys_addr -= KERNEL_VIRTUAL_BASE;
    }
    paging_set_cr3(phys_addr);
}

page_directory_t *page_directory_get_current(void) {
    u32 phys = paging_get_cr3();
    if (kernel_page_directory && phys == (u32)(uintptr_t)kernel_page_directory) {
        return kernel_page_directory;
    }
    return (page_directory_t *)(KERNEL_VIRTUAL_BASE + phys);
}

page_directory_t *page_directory_get_kernel(void) {
    return kernel_page_directory;
}

void page_directory_switch_to_kernel(void) {
    if (kernel_page_directory != NULL) {
        page_directory_switch(kernel_page_directory);
    }
}

/*
 * Page Mapping Operations
 */

bool page_map(page_directory_t *pd, u32 virt_addr, u32 phys_addr, u16 flags) {
    if (!pd || !is_page_aligned(virt_addr) || !is_page_aligned(phys_addr)) {
        return false;
    }
    
    u32 pd_idx = virt_to_pd_index(virt_addr);
    u32 pt_idx = virt_to_pt_index(virt_addr);
    
    page_table_t *pt;
    if (!(pd->entries[pd_idx] & PTE_PRESENT)) {
        /* Allocate new page table */
        u32 pt_phys = frame_alloc();
        if (!pt_phys) return false;
        
        u16 pde_flags = PTE_PRESENT | PTE_RW | PTE_ALLOCATED;
        if (flags & PTE_USER) pde_flags |= PTE_USER;
        pd->entries[pd_idx] = entry_create(pt_phys, pde_flags);
        
        pt = ptable_ptr( pt_phys);
        memset(pt, 0, sizeof(page_table_t));
    } else {
        u32 pt_phys = entry_get_phys(pd->entries[pd_idx]);
        pt = ptable_ptr( pt_phys);
        
        /* Ensure PDE has user bit if mapping a user page */
        if ((flags & PTE_USER) && !(pd->entries[pd_idx] & PTE_USER)) {
            pd->entries[pd_idx] |= PTE_USER;
        }
    }
    
    /* Set the page table entry */
    pt->entries[pt_idx] = entry_create(phys_addr, flags | PTE_PRESENT | PTE_ALLOCATED);

    
    /* Invalidate TLB for this page */
    paging_invalidate_tlb(virt_addr);
    
    return true;
}

bool page_map_existing(page_directory_t *pd, u32 virt_addr, u32 phys_addr, u16 flags) {
    if (!pd || !is_page_aligned(virt_addr) || !is_page_aligned(phys_addr)) {
        console_printf("[pme_fail] pd=%p v=%x p=%x\n", (void*)pd, virt_addr, phys_addr);
        return false;
    }

    u32 pd_idx = virt_to_pd_index(virt_addr);
    u32 pt_idx = virt_to_pt_index(virt_addr);

    page_table_t *pt;
    if (!(pd->entries[pd_idx] & PTE_PRESENT)) {
        u32 pt_phys = frame_alloc();
        if (!pt_phys) return false;

        u16 pde_flags = PTE_PRESENT | PTE_RW | PTE_ALLOCATED;
        if (flags & PTE_USER) pde_flags |= PTE_USER;
        pd->entries[pd_idx] = entry_create(pt_phys, pde_flags);
        pt = ptable_ptr( pt_phys);
        memset(pt, 0, sizeof(page_table_t));
    } else {
        u32 pt_phys = entry_get_phys(pd->entries[pd_idx]);
        pt = ptable_ptr( pt_phys);

        if ((flags & PTE_USER) && !(pd->entries[pd_idx] & PTE_USER)) {
            pd->entries[pd_idx] |= PTE_USER;
        }
    }

    pt->entries[pt_idx] = entry_create(phys_addr, flags | PTE_PRESENT);
    
    /* Invalidate TLB for this page */
    paging_invalidate_tlb(virt_addr);
    
    return true;
}

bool page_unmap(page_directory_t *pd, u32 virt_addr) {
    if (!pd || !is_page_aligned(virt_addr)) {
        return false;
    }
    
    u32 pd_idx = virt_to_pd_index(virt_addr);
    u32 pt_idx = virt_to_pt_index(virt_addr);
    
    if (!(pd->entries[pd_idx] & PTE_PRESENT)) {
        return false;
    }
    
    u32 pt_phys = entry_get_phys(pd->entries[pd_idx]);
    page_table_t *pt = ptable_ptr( pt_phys);
    paging_invalidate_tlb(virt_addr);
    
    return true;
}

u32 page_get_phys(page_directory_t *pd, u32 virt_addr) {
    if (!pd) pd = page_directory_get_current();
    
    u32 pd_idx = virt_to_pd_index(virt_addr);
    
    if (!(pd->entries[pd_idx] & PTE_PRESENT)) {
        return 0;
    }
    
    u32 pt_phys = entry_get_phys(pd->entries[pd_idx]);
    page_table_t *pt = ptable_ptr( pt_phys);
    u32 pt_idx = virt_to_pt_index(virt_addr);
    
    if (!(pt->entries[pt_idx] & PTE_PRESENT)) {
        return 0;
    }
    
    return entry_get_phys(pt->entries[pt_idx]) + page_offset(virt_addr);
}

bool page_is_present(page_directory_t *pd, u32 virt_addr) {
    if (!pd) pd = page_directory_get_current();
    
    u32 pd_idx = virt_to_pd_index(virt_addr);
    
    if (!(pd->entries[pd_idx] & PTE_PRESENT)) {
        return false;
    }
    
    u32 pt_phys = entry_get_phys(pd->entries[pd_idx]);
    page_table_t *pt = ptable_ptr( pt_phys);
    u32 pt_idx = virt_to_pt_index(virt_addr);
    
    return (pt->entries[pt_idx] & PTE_PRESENT) != 0;
}

bool page_set_flags(page_directory_t *pd, u32 virt_addr, u16 flags) {
    if (!pd) pd = page_directory_get_current();
    
    u32 pd_idx = virt_to_pd_index(virt_addr);
    
    if (!(pd->entries[pd_idx] & PTE_PRESENT)) {
        return false;
    }
    
    /* Ensure PDE has user bit if setting user permission */
    if ((flags & PTE_USER) && !(pd->entries[pd_idx] & PTE_USER)) {
        pd->entries[pd_idx] |= PTE_USER;
    }
    
    u32 pt_phys = entry_get_phys(pd->entries[pd_idx]);
    page_table_t *pt = ptable_ptr( pt_phys);
    u32 pt_idx = virt_to_pt_index(virt_addr);
    
    if (!(pt->entries[pt_idx] & PTE_PRESENT)) {
        return false;
    }
    
    u32 phys = entry_get_phys(pt->entries[pt_idx]);
    pt->entries[pt_idx] = entry_create(phys, flags | PTE_ALLOCATED);
    paging_invalidate_tlb(virt_addr);
    
    return true;
}

/*
 * Kernel Page Operations
 */

bool paging_map_kernel_range(page_directory_t *pd, u32 virt_start, 
                              u32 phys_start, size_t size, u16 flags) {
    if (!is_page_aligned(virt_start) || !is_page_aligned(phys_start)) {
        return false;
    }
    
    for (u32 offset = 0; offset < size; offset += PAGE_SIZE) {
        if (!page_map(pd, virt_start + offset, phys_start + offset, 
                      flags | PTE_GLOBAL)) {
            return false;
        }
    }
    return true;
}

void paging_map_kernel(page_directory_t *pd) {
    if (!pd) return;
    
    /* Copy all kernel directory entries */
    for (int i = KERNEL_PD_INDEX; i < ENTRIES_PER_PD; i++) {
        pd->entries[i] = kernel_page_directory->entries[i];
    }
}

/*
 * User Page Operations
 */

bool paging_alloc_user_page(page_directory_t *pd, u32 virt_addr, u16 flags) {
    if (!pd) return false;
    
    /* Ensure address is in user space */
    if (virt_addr >= KERNEL_VIRTUAL_BASE) {
        return false;
    }
    
    /* Allocate physical frame */
    u32 phys = frame_alloc();
    if (!phys) return false;
    
    /* Map with user-accessible flag */
    if (!page_map(pd, virt_addr, phys, flags | PTE_USER)) {
        frame_free(phys);
        return false;
    }
    
    
    return true;
}

bool paging_free_user_page(page_directory_t *pd, u32 virt_addr) {
    if (!pd) return false;
    
    /* Ensure address is in user space */
    if (virt_addr >= KERNEL_VIRTUAL_BASE) {
        return false;
    }
    
    return page_unmap(pd, virt_addr);
}

/*
 * Copy-on-Write Page Copying
 */

bool paging_copy_page(page_directory_t *src_pd, page_directory_t *dst_pd, 
                       u32 virt_addr, bool cow) {
    if (!src_pd || !dst_pd) return false;
    if (virt_addr >= KERNEL_VIRTUAL_BASE) return false;
    
    u32 pd_idx = virt_to_pd_index(virt_addr);
    u32 pt_idx = virt_to_pt_index(virt_addr);
    
    /* Check source page exists */
    if (!(src_pd->entries[pd_idx] & PTE_PRESENT)) {
        return false;
    }
    
    u32 src_pt_phys = entry_get_phys(src_pd->entries[pd_idx]);
    page_table_t *src_pt = ptable_ptr( src_pt_phys);
    
    if (!(src_pt->entries[pt_idx] & PTE_PRESENT)) {
        return false;
    }
    
    u32 src_flags = entry_get_flags(src_pt->entries[pt_idx]);
    u32 src_phys = entry_get_phys(src_pt->entries[pt_idx]);
    
    if (cow) {
        /* Copy-on-write: share page read-only */
        
        /* Mark source as read-only if writable */
        if (src_flags & PTE_RW) {
            src_pt->entries[pt_idx] = entry_create(src_phys, 
                (src_flags & ~PTE_RW) | PTE_COW);
            paging_invalidate_tlb(virt_addr);
        }
        
        /* Map destination to same physical page */
        u32 dst_flags = src_flags | PTE_COW;
        if (!page_map(dst_pd, virt_addr, src_phys, dst_flags & ~PTE_RW)) {
            return false;
        }
    } else {
        /* Full copy: allocate new frame and copy data */
        u32 dst_phys = frame_alloc();
        if (!dst_phys) return false;
        
        /* Map temporarily to copy */
        u32 temp_virt = KERNEL_VIRTUAL_BASE - PAGE_SIZE;
        page_map(kernel_page_directory, temp_virt, dst_phys, PTE_RW);
        
        /* Copy page data */
        memcpy((void *)temp_virt, (void *)(uintptr_t)src_phys, PAGE_SIZE);
        
        /* Unmap temporary */
        page_unmap(kernel_page_directory, temp_virt);
        
        /* Map in destination with original flags */
        if (!page_map(dst_pd, virt_addr, dst_phys, src_flags)) {
            frame_free(dst_phys);
            return false;
        }
    }
    
    return true;
}

/*
 * Demand Paging Support
 */

/* Set up a page for demand-zero allocation (anonymous) */
bool paging_setup_demand_anon(page_directory_t *pd, u32 virt_addr, u16 flags) {
    if (!pd || virt_addr >= KERNEL_VIRTUAL_BASE) return false;
    
    /* Just create the page table entry without allocating a frame */
    /* Mark as not present but with demand-zero flag */
    u32 pd_idx = virt_to_pd_index(virt_addr);
    u32 pt_idx = virt_to_pt_index(virt_addr);
    
    /* Get or create page table */
    page_table_t *pt;
    if (!(pd->entries[pd_idx] & PTE_PRESENT)) {
        u32 pt_phys = frame_alloc();
        if (!pt_phys) return false;
        u16 pde_flags = PTE_PRESENT | PTE_RW | PTE_ALLOCATED;
        if (flags & PTE_USER) pde_flags |= PTE_USER;
        pd->entries[pd_idx] = entry_create(pt_phys, pde_flags);
        pt = ptable_ptr( pt_phys);
        memset(pt, 0, sizeof(page_table_t));
    } else {
        u32 pt_phys = entry_get_phys(pd->entries[pd_idx]);
        pt = ptable_ptr( pt_phys);
        
        if ((flags & PTE_USER) && !(pd->entries[pd_idx] & PTE_USER)) {
            pd->entries[pd_idx] |= PTE_USER;
        }
    }
    
    /* Set up as not-present with demand-zero flag */
    /* We store the desired flags in the entry, but mark as not present */
    pt->entries[pt_idx] = (flags | PTE_DEMAND_ZERO) & ~PTE_PRESENT;
    
    return true;
}

/* Handle demand-zero page fault - allocate and zero a page */
static bool handle_demand_anon(page_directory_t *pd, u32 virt_addr, bool for_write) {
    u32 pd_idx = virt_to_pd_index(virt_addr);
    u32 pt_idx = virt_to_pt_index(virt_addr);
    
    if (!(pd->entries[pd_idx] & PTE_PRESENT)) return false;
    
    u32 pt_phys = entry_get_phys(pd->entries[pd_idx]);
    page_table_t *pt = ptable_ptr( pt_phys);
    
    u32 entry = pt->entries[pt_idx];
    if ((entry & PTE_PRESENT) || !(entry & PTE_DEMAND_ZERO)) return false;
    
    /* Extract desired flags */
    u16 desired_flags = entry_get_flags(entry) & ~PTE_DEMAND_ZERO;
    
    /* Allocate frame */
    u32 phys = frame_alloc();
    if (!phys) return false;
    
    /* Zero the page (demand-zero semantics) */
    u32 temp_virt = KERNEL_VIRTUAL_BASE - PAGE_SIZE * 3;
    page_map(kernel_page_directory, temp_virt, phys, PTE_RW);
    memset((void *)temp_virt, 0, PAGE_SIZE);
    page_unmap(kernel_page_directory, temp_virt);
    
    /* Update page table entry */
    pt->entries[pt_idx] = entry_create(phys, desired_flags | PTE_PRESENT | PTE_ALLOCATED);
    paging_invalidate_tlb(virt_addr);
    
    (void)for_write; /* for_write could adjust flags in future */
    return true;
}

/*
 * Page Fault Handler
 */

void page_fault_handler(u32 virt_addr, u32 error_code, registers_t *regs) {
    bool present = error_code & PF_ERR_PRESENT;
    bool rw = error_code & PF_ERR_RW;
    bool user = error_code & PF_ERR_USER;
    bool reserved = error_code & PF_ERR_RESERVED;
    bool fetch = error_code & PF_ERR_FETCH;
    
    /* Get current page directory */
    page_directory_t *pd = page_directory_get_current();
    
    /* Check for copy-on-write */
    if (present && rw && !reserved) {
        u32 pd_idx = virt_to_pd_index(virt_addr);
        u32 pt_idx = virt_to_pt_index(virt_addr);
        
        if (pd->entries[pd_idx] & PTE_PRESENT) {
            u32 pt_phys = entry_get_phys(pd->entries[pd_idx]);
            page_table_t *pt = ptable_ptr( pt_phys);
            
            if ((pt->entries[pt_idx] & PTE_PRESENT) && 
                (pt->entries[pt_idx] & PTE_COW)) {
                
                /* Copy the page */
                u32 old_phys = entry_get_phys(pt->entries[pt_idx]);
                u32 new_phys = frame_alloc();
                
                if (!new_phys) {
                    console_printf("[page_fault] OOM in COW handler\n");
                    task_abort_from_trap(regs, "page fault (OOM)");
                    return;
                }
                
                /* Map temporarily to copy */
                u32 temp_virt = KERNEL_VIRTUAL_BASE - PAGE_SIZE * 2;
                page_map(kernel_page_directory, temp_virt, new_phys, PTE_RW);
                memcpy((void *)temp_virt, (void *)(uintptr_t)old_phys, PAGE_SIZE);
                page_unmap(kernel_page_directory, temp_virt);
                
                /* Update mapping to new frame with write permission */
                u32 flags = entry_get_flags(pt->entries[pt_idx]);
                pt->entries[pt_idx] = entry_create(new_phys, 
                    (flags & ~PTE_COW) | PTE_RW | PTE_ALLOCATED);
                paging_invalidate_tlb(virt_addr);
                
                return; /* Success */
            }
        }
    }
    
    /* Handle demand-zero paging */
    if (!present && user && virt_addr < KERNEL_VIRTUAL_BASE) {
        /* Check if this is a demand-zero page */
        u32 pd_idx = virt_to_pd_index(virt_addr);
        
        if (pd->entries[pd_idx] & PTE_PRESENT) {
            u32 pt_phys = entry_get_phys(pd->entries[pd_idx]);
            page_table_t *pt = ptable_ptr( pt_phys);
            u32 pt_idx = virt_to_pt_index(virt_addr);
            
            if ((pt->entries[pt_idx] & PTE_DEMAND_ZERO) && !(pt->entries[pt_idx] & PTE_PRESENT)) {
                if (handle_demand_anon(pd, virt_addr, rw)) {
                    return; /* Success - page allocated */
                }
            }
        }
        
        /* Unmapped user address - this is a real fault */
        console_printf("[page_fault] unmapped user address %x\n", virt_addr);
    }
    
    /* Log the fault and kill the process */
    console_printf("[page_fault] %s at %x by %s (%s%s%s%s)\n",
                   present ? "protection violation" : "page not present",
                   virt_addr,
                   user ? "user" : "kernel",
                   rw ? "write " : "read ",
                   reserved ? "reserved-bit " : "",
                   fetch ? "fetch " : "");
    
    if (user && task_is_active()) {
        task_abort_from_trap(regs, "page fault");
    } else {
        panic("Kernel page fault at %x", virt_addr);
    }
}

/*
 * Initialization
 */

/* Parse multiboot memory map to find best region for user frames */
static bool find_best_memory_region(const multiboot_info_t *mbi, 
                                     u32 *out_start, u32 *out_end, 
                                     u32 kernel_end_phys) {
    u32 best_start = 0;
    u32 best_size = 0;
    
    if (!(mbi->flags & MULTIBOOT_FLAG_MMAP)) {
        /* Fallback to simple calculation */
        *out_start = (kernel_end_phys + 0xFFFFFF) & ~0xFFFFFF; /* Align to 16MB */
        if (*out_start < 0x1000000) *out_start = 0x1000000; /* Min 16MB */
        /* Use memory info from multiboot if available */
        if (mbi->flags & MULTIBOOT_FLAG_MEM) {
            *out_end = (mbi->mem_lower + mbi->mem_upper) * 1024;
        } else {
            *out_end = kernel_end_phys + 0x10000000; /* Assume 256MB */
        }
        return true;
    }
    
    /* Walk memory map to find largest available region above kernel */
    u32 mmap_addr = mbi->mmap_addr;
    u32 mmap_end = mbi->mmap_addr + mbi->mmap_length;
    
    while (mmap_addr < mmap_end) {
        multiboot_mmap_entry_t *entry = (multiboot_mmap_entry_t *)(uintptr_t)mmap_addr;
        
        if (entry->type == 1 && entry->addr_high == 0) {
            u32 region_start = entry->addr_low;
            u32 region_len = entry->len_low;
            u32 region_end = region_start + region_len;
            
            /* Skip low memory (below 1MB) */
            if (region_end <= 0x100000) {
                mmap_addr += entry->size + sizeof(entry->size);
                continue;
            }
            
            /* Skip region if it overlaps with kernel */
            if (region_start < kernel_end_phys && region_end > 0x100000) {
                region_start = kernel_end_phys;
            }
            
            /* Align start to page boundary */
            region_start = (region_start + PAGE_SIZE - 1) & PAGE_MASK;
            
            if (region_start < region_end) {
                u32 size = region_end - region_start;
                if (size > best_size) {
                    best_start = region_start;
                    best_size = size;
                }
            }
        }
        
        mmap_addr += entry->size + sizeof(entry->size);
    }
    
    if (best_size >= 4 * 1024 * 1024) { /* At least 4MB */
        *out_start = best_start;
        *out_end = best_start + best_size;
        return true;
    }
    
    /* Fallback */
    *out_start = 0x10000000; /* 256MB */
    *out_end = kernel_end_phys + 0x10000000; /* Assume 256MB available after kernel */
    if (*out_end < 0x2000000) *out_end = 0x2000000; /* At least 32MB */
    return true;
}

void paging_init(const multiboot_info_t *mbi, u32 total_memory_bytes, u32 kernel_end_phys) {
    console_write("[paging] initializing...\n");
    
    /* Find best memory region for user frames */
    u32 user_mem_start, user_mem_end;
    find_best_memory_region(mbi, &user_mem_start, &user_mem_end, kernel_end_phys);

    /* Ensure frame allocator does not overlap with kernel heap.
     * The kernel heap (kmalloc) manages physical memory from kernel_end
     * up to heap_end via identity mapping.  The frame allocator must
     * start after the kernel heap to avoid double-allocating the same
     * physical frames (which corrupts page directories, ELF buffers, etc.). */
    {
        u32 heap_end = memory_heap_end();
        if (heap_end > 0 && user_mem_start < heap_end) {
            user_mem_start = (heap_end + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);
            if (user_mem_start >= user_mem_end) {
                panic("Frame allocator region exhausted by kernel heap");
            }
        }
    }

    /* Initialize frame allocator with actual available memory */
    u32 first_frame = user_mem_start / PAGE_SIZE;
    
    console_printf("[paging] using memory region %x - %x (%u MiB)\n",
                   user_mem_start, user_mem_end, 
                   (user_mem_end - user_mem_start) / (1024 * 1024));
    
    if (!frame_alloc_init(user_mem_start, user_mem_end, first_frame)) {
        panic("Failed to initialize frame allocator");
    }
    
    /* Allocate kernel page directory */
    kernel_page_directory = kmalloc_aligned(sizeof(page_directory_t), PAGE_SIZE);
    if (!kernel_page_directory) {
        panic("Failed to allocate kernel page directory");
    }
    memset(kernel_page_directory, 0, sizeof(page_directory_t));
    
    /* Identity map first 4MB (boot + kernel code + initial heap) */
    for (u32 addr = 0; addr < 0x400000; addr += PAGE_SIZE) {
        page_map(kernel_page_directory, addr, addr, PTE_PRESENT | PTE_RW | PTE_GLOBAL);
    }

    /* Identity map 4–16 MB gap so the kernel heap (which may span this range)
     * remains accessible after paging is enabled. */
    for (u32 addr = 0x400000; addr < 0x01000000; addr += PAGE_SIZE) {
        page_map(kernel_page_directory, addr, addr, PTE_PRESENT | PTE_RW | PTE_GLOBAL);
    }
    
    /* Map kernel heap region */
    for (u32 addr = 0x01000000; addr < 0x10000000; addr += PAGE_SIZE) {
        page_map(kernel_page_directory, addr, addr, PTE_PRESENT | PTE_RW | PTE_GLOBAL);
    }
    
    /* Set up higher-half mapping at 3GB */
    /* Map kernel virtual 0xC0000000 -> physical 0x00000000 (first 1GB) */
    for (u32 i = 0; i < 0x40000000; i += PAGE_SIZE) {
        page_map(kernel_page_directory, KERNEL_VIRTUAL_BASE + i, i, 
                 PTE_PRESENT | PTE_RW | PTE_GLOBAL);
    }
    
    /* Identity-map the framebuffer (if present) before enabling paging,
     * so console writes via libtsm don't page fault. */
    if (mbi->flags & MULTIBOOT_FLAG_FRAMEBUFFER) {
        u64 fb_phys = mbi->framebuffer_addr;
        if (fb_phys != 0 && (fb_phys >> 32) == 0) {
            u32 fb_phys32 = (u32)fb_phys;
            u32 fb_size = mbi->framebuffer_pitch * mbi->framebuffer_height;
            if (fb_size > 0) {
                fb_size = (fb_size + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);
                for (u32 offset = 0; offset < fb_size; offset += PAGE_SIZE) {
                    page_map_existing(kernel_page_directory,
                                      fb_phys32 + offset, fb_phys32 + offset,
                                      PTE_PRESENT | PTE_RW);
                }
                paging_flush_tlb();
            }
        }
    }

    /* Switch to kernel page directory */
    kernel_page_directory_phys = (u32)(uintptr_t)kernel_page_directory;
    paging_set_cr3(kernel_page_directory_phys);
    
    console_printf("[paging] kernel page directory at %x (phys %x)\n",
                   (u32)kernel_page_directory, kernel_page_directory_phys);
    console_printf("[paging] PDE[0]=%x PDE[1]=%x PDE[2]=%x PDE[3]=%x PDE[4]=%x\n",
                   kernel_page_directory->entries[0],
                   kernel_page_directory->entries[1],
                   kernel_page_directory->entries[2],
                   kernel_page_directory->entries[3],
                   kernel_page_directory->entries[4]);
    console_printf("[paging] enabling paging...\n");
    paging_enable();
    paging_active = true;
    console_printf("[paging] ready - %u frames total, %u free\n", 
                   frame_total(), frame_free_count());
}

/* Helper to get kernel page directory for copying */
page_directory_t *paging_get_kernel_pd(void) {
    return kernel_page_directory;
}
