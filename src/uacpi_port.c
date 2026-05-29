#include <uacpi/kernel_api.h>
#include <uacpi/uacpi.h>
#include <uacpi/tables.h>
#include <uacpi/sleep.h>

#include "console.h"
#include "io.h"
#include "memory.h"
#include "paging.h"
#include "pci.h"
#include "pit.h"
#include "idt.h"
#include "string.h"
#include "panic.h"

#define KERNEL_VIRTUAL_BASE 0xC0000000u

static bool uacpi_port_initialized;
static uacpi_phys_addr uacpi_rsdp_address;

struct uacpi_mapping {
    void *virt;
    uacpi_phys_addr phys;
    uacpi_size size;
    bool used;
};
static struct uacpi_mapping uacpi_mappings[16];

static void *map_via_kernel(uacpi_phys_addr phys, uacpi_size len) {
    u32 phys32 = (u32)phys;
    u32 offset = phys32 & (PAGE_SIZE - 1);
    u32 phys_page = phys32 & PAGE_MASK;
    u32 virt_page = KERNEL_VIRTUAL_BASE + phys_page;
    return (void *)(uintptr_t)(virt_page + offset);
}

static void *allocate_mapping(uacpi_phys_addr phys, uacpi_size len) {
    for (int i = 0; i < 16; i++) {
        if (!uacpi_mappings[i].used) {
            uacpi_phys_addr phys_page = phys & ~(uacpi_phys_addr)(PAGE_SIZE - 1);
            u32 offset = (u32)(phys - phys_page);
            u32 map_size = (u32)((len + offset + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));

            u32 window_base = KERNEL_VIRTUAL_BASE + 0x80000000u;
            u32 window_top = 0xFFC00000u;

            u32 pages_needed = map_size / PAGE_SIZE;
            if (pages_needed == 0) pages_needed = 1;
            u32 window_size = pages_needed * PAGE_SIZE;

            u32 va = window_top - window_size;
            if (va < window_base) return NULL;

            page_directory_t *pd = page_directory_get_kernel();
            for (u32 page = 0; page < window_size; page += PAGE_SIZE) {
                if (!page_map_existing(pd, va + page,
                                       (u32)(phys_page + page),
                                       PTE_PRESENT | PTE_RW | PTE_GLOBAL)) {
                    for (u32 cleanup = 0; cleanup < page; cleanup += PAGE_SIZE) {
                        page_unmap(pd, va + cleanup);
                    }
                    return NULL;
                }
            }
            paging_flush_tlb();

            uacpi_mappings[i].virt = (void *)(uintptr_t)(va + offset);
            uacpi_mappings[i].phys = phys;
            uacpi_mappings[i].size = len;
            uacpi_mappings[i].used = true;
            return uacpi_mappings[i].virt;
        }
    }
    return NULL;
}

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp_address) {
    if (uacpi_rsdp_address != 0) {
        *out_rsdp_address = uacpi_rsdp_address;
        return UACPI_STATUS_OK;
    }

    u16 ebda_segment = *(volatile u16 *)(uintptr_t)0x40E;
    u32 ebda_base = (u32)ebda_segment << 4;

    u32 ranges[][2] = {
        {ebda_base, ebda_base + 1024},
        {0xE0000u, 0x100000u},
    };
    int num_ranges = 2;

    if (ebda_base < 0x80000u || ebda_base >= 0xA0000u) {
        ranges[0][1] = 0;
        num_ranges = 1;
    }

    for (int r = 0; r < num_ranges; r++) {
        u32 start = ranges[r][0];
        u32 end = ranges[r][1];
        if (start >= end) continue;

        for (u32 addr = start; addr + 20 <= end; addr += 16) {
            const char *sig = (const char *)(uintptr_t)addr;
            if (sig[0] == 'R' && sig[1] == 'S' && sig[2] == 'D' &&
                sig[3] == ' ' && sig[4] == 'P' && sig[5] == 'T' &&
                sig[6] == 'R' && sig[7] == ' ') {
                u8 sum = 0;
                for (int i = 0; i < 20; i++) sum = (u8)(sum + ((const u8 *)(uintptr_t)addr)[i]);
                if (sum == 0) {
                    uacpi_rsdp_address = addr;
                    *out_rsdp_address = addr;
                    return UACPI_STATUS_OK;
                }
            }
        }
    }
    return UACPI_STATUS_NOT_FOUND;
}

void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len) {
    u32 phys_max = (u32)addr + (u32)len;
    if (phys_max <= 0x40000000u) {
        return map_via_kernel(addr, len);
    }
    return allocate_mapping(addr, len);
}

void uacpi_kernel_unmap(void *addr, uacpi_size len) {
    (void)len;
    for (int i = 0; i < 16; i++) {
        if (uacpi_mappings[i].used && uacpi_mappings[i].virt == addr) {
            uacpi_phys_addr phys_page = uacpi_mappings[i].phys & ~(uacpi_phys_addr)(PAGE_SIZE - 1);
            u32 offset = (u32)(uacpi_mappings[i].phys - phys_page);
            u32 map_size = (u32)((uacpi_mappings[i].size + offset + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));
            u32 va_page = (u32)(uintptr_t)addr & PAGE_MASK;

            page_directory_t *pd = page_directory_get_kernel();
            for (u32 page = 0; page < map_size; page += PAGE_SIZE) {
                page_unmap(pd, va_page + page);
            }
            paging_flush_tlb();

            uacpi_mappings[i].used = false;
            return;
        }
    }
}

void uacpi_kernel_log(uacpi_log_level level, const uacpi_char *message) {
    const char *prefix;
    switch (level) {
    case UACPI_LOG_ERROR:   prefix = "[uacpi] ERROR: "; break;
    case UACPI_LOG_WARN:    prefix = "[uacpi] WARN:  "; break;
    case UACPI_LOG_INFO:    prefix = "[uacpi] INFO:  "; break;
    case UACPI_LOG_DEBUG:   prefix = "[uacpi] DEBUG: "; break;
    case UACPI_LOG_TRACE:   prefix = "[uacpi] TRACE: "; break;
    default:                prefix = "[uacpi] ";       break;
    }
    console_write(prefix);
    console_write(message);
    console_putc('\n');
}

uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address address, uacpi_handle *out_handle) {
    uacpi_pci_address *dev = uacpi_kernel_alloc(sizeof(uacpi_pci_address));
    if (!dev) return UACPI_STATUS_OUT_OF_MEMORY;
    *dev = address;
    *out_handle = dev;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_pci_device_close(uacpi_handle handle) {
    uacpi_kernel_free(handle);
}

static u32 pci_read_raw(uacpi_handle device, uacpi_size offset, uacpi_size width) {
    uacpi_pci_address *addr = (uacpi_pci_address *)device;
    return pci_config_read(addr->bus, addr->device, addr->function, (u8)offset);
}

static void pci_write_raw(uacpi_handle device, uacpi_size offset, uacpi_size width, u32 value) {
    uacpi_pci_address *addr = (uacpi_pci_address *)device;
    if (width == 4) {
        pci_config_write(addr->bus, addr->device, addr->function, (u8)offset, value);
    } else {
        u32 orig = pci_config_read(addr->bus, addr->device, addr->function, (u8)(offset & ~3));
        u32 shift = (offset & 3) * 8;
        u32 mask = (width == 1) ? 0xFF : 0xFFFF;
        mask <<= shift;
        orig = (orig & ~mask) | ((value & 0xFF) << shift);
        pci_config_write(addr->bus, addr->device, addr->function, (u8)(offset & ~3), orig);
    }
}

uacpi_status uacpi_kernel_pci_read8(uacpi_handle device, uacpi_size offset, uacpi_u8 *value) {
    *value = (uacpi_u8)(pci_read_raw(device, offset, 1));
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_pci_read16(uacpi_handle device, uacpi_size offset, uacpi_u16 *value) {
    *value = (uacpi_u16)(pci_read_raw(device, offset, 2));
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_pci_read32(uacpi_handle device, uacpi_size offset, uacpi_u32 *value) {
    *value = pci_read_raw(device, offset, 4);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_pci_write8(uacpi_handle device, uacpi_size offset, uacpi_u8 value) {
    pci_write_raw(device, offset, 1, value);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_pci_write16(uacpi_handle device, uacpi_size offset, uacpi_u16 value) {
    pci_write_raw(device, offset, 2, value);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_pci_write32(uacpi_handle device, uacpi_size offset, uacpi_u32 value) {
    pci_write_raw(device, offset, 4, value);
    return UACPI_STATUS_OK;
}

struct uacpi_io_range {
    uacpi_io_addr base;
    uacpi_size len;
};

uacpi_status uacpi_kernel_io_map(uacpi_io_addr base, uacpi_size len, uacpi_handle *out_handle) {
    struct uacpi_io_range *range = uacpi_kernel_alloc(sizeof(struct uacpi_io_range));
    if (!range) return UACPI_STATUS_OUT_OF_MEMORY;
    range->base = base;
    range->len = len;
    *out_handle = range;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle handle) {
    uacpi_kernel_free(handle);
}

uacpi_status uacpi_kernel_io_read8(uacpi_handle handle, uacpi_size offset, uacpi_u8 *out_value) {
    struct uacpi_io_range *range = (struct uacpi_io_range *)handle;
    *out_value = inb((u16)(range->base + offset));
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_read16(uacpi_handle handle, uacpi_size offset, uacpi_u16 *out_value) {
    struct uacpi_io_range *range = (struct uacpi_io_range *)handle;
    *out_value = inw((u16)(range->base + offset));
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_read32(uacpi_handle handle, uacpi_size offset, uacpi_u32 *out_value) {
    struct uacpi_io_range *range = (struct uacpi_io_range *)handle;
    *out_value = inl((u16)(range->base + offset));
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_write8(uacpi_handle handle, uacpi_size offset, uacpi_u8 in_value) {
    struct uacpi_io_range *range = (struct uacpi_io_range *)handle;
    outb((u16)(range->base + offset), in_value);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_write16(uacpi_handle handle, uacpi_size offset, uacpi_u16 in_value) {
    struct uacpi_io_range *range = (struct uacpi_io_range *)handle;
    outw((u16)(range->base + offset), in_value);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_write32(uacpi_handle handle, uacpi_size offset, uacpi_u32 in_value) {
    struct uacpi_io_range *range = (struct uacpi_io_range *)handle;
    outl((u16)(range->base + offset), in_value);
    return UACPI_STATUS_OK;
}

void *uacpi_kernel_alloc(uacpi_size size) {
    return kmalloc((size_t)size);
}

void uacpi_kernel_free(void *mem) {
    kfree(mem);
}

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void) {
    u32 ticks = pit_ticks();
    return (uacpi_u64)ticks * 10000000ULL;
}

void uacpi_kernel_stall(uacpi_u8 usec) {
    volatile u32 loops = (u32)usec * 100;
    while (loops--) {
        __asm__ volatile ("pause");
    }
}

void uacpi_kernel_sleep(uacpi_u64 msec) {
    u32 ticks = (u32)(msec / 10);
    if (ticks > 0) {
        pit_sleep(ticks);
    } else {
        uacpi_kernel_stall((uacpi_u8)(msec * 1000));
    }
}

typedef struct {
    volatile int locked;
} uacpi_mutex_t;

uacpi_handle uacpi_kernel_create_mutex(void) {
    uacpi_mutex_t *mtx = uacpi_kernel_alloc(sizeof(uacpi_mutex_t));
    if (mtx) mtx->locked = 0;
    return mtx;
}

void uacpi_kernel_free_mutex(uacpi_handle handle) {
    uacpi_kernel_free(handle);
}

uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle handle, uacpi_u16 timeout) {
    uacpi_mutex_t *mtx = (uacpi_mutex_t *)handle;
    u32 deadline = pit_ticks() + (timeout / 10);

    while (1) {
        int expected = 0;
        if (__atomic_compare_exchange_n(&mtx->locked, &expected, 1, 0,
                                         __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
            return UACPI_STATUS_OK;
        }
        if (timeout == 0) return UACPI_STATUS_TIMEOUT;
        if (timeout != 0xFFFF && pit_ticks() >= deadline) return UACPI_STATUS_TIMEOUT;
        __asm__ volatile ("pause");
    }
}

void uacpi_kernel_release_mutex(uacpi_handle handle) {
    uacpi_mutex_t *mtx = (uacpi_mutex_t *)handle;
    __atomic_store_n(&mtx->locked, 0, __ATOMIC_RELEASE);
}

typedef struct {
    volatile int value;
} uacpi_event_t;

uacpi_handle uacpi_kernel_create_event(void) {
    uacpi_event_t *ev = uacpi_kernel_alloc(sizeof(uacpi_event_t));
    if (ev) ev->value = 0;
    return ev;
}

void uacpi_kernel_free_event(uacpi_handle handle) {
    uacpi_kernel_free(handle);
}

uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle handle, uacpi_u16 timeout) {
    uacpi_event_t *ev = (uacpi_event_t *)handle;
    u32 deadline = (timeout != 0xFFFF) ? (pit_ticks() + timeout / 10) : 0;

    while (1) {
        int val = __atomic_load_n(&ev->value, __ATOMIC_ACQUIRE);
        if (val > 0) {
            __atomic_sub_fetch(&ev->value, 1, __ATOMIC_RELEASE);
            return UACPI_TRUE;
        }
        if (timeout == 0) return UACPI_FALSE;
        if (timeout != 0xFFFF && pit_ticks() >= deadline) return UACPI_FALSE;
        __asm__ volatile ("pause");
    }
}

void uacpi_kernel_signal_event(uacpi_handle handle) {
    uacpi_event_t *ev = (uacpi_event_t *)handle;
    __atomic_add_fetch(&ev->value, 1, __ATOMIC_RELEASE);
}

void uacpi_kernel_reset_event(uacpi_handle handle) {
    uacpi_event_t *ev = (uacpi_event_t *)handle;
    __atomic_store_n(&ev->value, 0, __ATOMIC_RELEASE);
}

uacpi_thread_id uacpi_kernel_get_thread_id(void) {
    return (uacpi_thread_id)(uintptr_t)1;
}

uacpi_interrupt_state uacpi_kernel_disable_interrupts(void) {
    u32 flags;
    __asm__ volatile ("pushfl; popl %0; cli" : "=r"(flags));
    return (uacpi_interrupt_state)(uintptr_t)flags;
}

void uacpi_kernel_restore_interrupts(uacpi_interrupt_state state) {
    u32 flags = (u32)(uintptr_t)state;
    if (flags & 0x200) {
        __asm__ volatile ("sti");
    }
}

struct uacpi_irq_entry {
    uacpi_u32 irq;
    uacpi_interrupt_handler handler;
    uacpi_handle ctx;
    bool used;
};
static struct uacpi_irq_entry uacpi_irqs[4];

static void uacpi_irq_dispatcher(registers_t *regs) {
    (void)regs;
    for (int i = 0; i < 4; i++) {
        if (uacpi_irqs[i].used) {
            uacpi_irqs[i].handler(uacpi_irqs[i].ctx);
        }
    }
}

uacpi_status uacpi_kernel_install_interrupt_handler(uacpi_u32 irq, uacpi_interrupt_handler handler, uacpi_handle ctx, uacpi_handle *out_irq_handle) {
    for (int i = 0; i < 4; i++) {
        if (!uacpi_irqs[i].used) {
            uacpi_irqs[i].irq = irq;
            uacpi_irqs[i].handler = handler;
            uacpi_irqs[i].ctx = ctx;
            uacpi_irqs[i].used = true;

            u8 pic_irq = (u8)(irq - 0x20);
            irq_install_handler(pic_irq, uacpi_irq_dispatcher);

            if (out_irq_handle) *out_irq_handle = (uacpi_handle)(uintptr_t)(i + 1);
            return UACPI_STATUS_OK;
        }
    }
    return UACPI_STATUS_OUT_OF_MEMORY;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(uacpi_interrupt_handler handler, uacpi_handle irq_handle) {
    int idx = (int)(uintptr_t)irq_handle - 1;
    if (idx >= 0 && idx < 4 && uacpi_irqs[idx].used) {
        uacpi_irqs[idx].used = false;
        irq_remove_handler((u8)(uacpi_irqs[idx].irq - 0x20));
    }
    return UACPI_STATUS_OK;
}

struct uacpi_spinlock {
    volatile int locked;
};

uacpi_handle uacpi_kernel_create_spinlock(void) {
    uacpi_mutex_t *lock = uacpi_kernel_alloc(sizeof(uacpi_mutex_t));
    if (lock) lock->locked = 0;
    return lock;
}

void uacpi_kernel_free_spinlock(uacpi_handle handle) {
    uacpi_kernel_free(handle);
}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle handle) {
    uacpi_mutex_t *lock = (uacpi_mutex_t *)handle;
    uacpi_cpu_flags flags = (uacpi_cpu_flags)uacpi_kernel_disable_interrupts();
    while (__atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE)) {
        __asm__ volatile ("pause");
    }
    return flags;
}

void uacpi_kernel_unlock_spinlock(uacpi_handle handle, uacpi_cpu_flags flags) {
    uacpi_mutex_t *lock = (uacpi_mutex_t *)handle;
    __atomic_clear(&lock->locked, __ATOMIC_RELEASE);
    uacpi_kernel_restore_interrupts((uacpi_interrupt_state)(uintptr_t)flags);
}

struct uacpi_work_item {
    uacpi_work_handler handler;
    uacpi_handle ctx;
    struct uacpi_work_item *next;
};
static struct uacpi_work_item *uacpi_work_queue;
static struct uacpi_work_item uacpi_work_pool[8];
static int uacpi_work_pool_used;

uacpi_status uacpi_kernel_schedule_work(uacpi_work_type type, uacpi_work_handler handler, uacpi_handle ctx) {
    (void)type;
    struct uacpi_work_item *item = NULL;

    if (uacpi_work_pool_used < 8) {
        item = &uacpi_work_pool[uacpi_work_pool_used++];
    } else {
        item = uacpi_kernel_alloc(sizeof(struct uacpi_work_item));
    }
    if (!item) return UACPI_STATUS_OUT_OF_MEMORY;

    item->handler = handler;
    item->ctx = ctx;
    item->next = NULL;

    uacpi_cpu_flags flags = uacpi_kernel_lock_spinlock((uacpi_handle)1);
    if (!uacpi_work_queue) {
        uacpi_work_queue = item;
    } else {
        struct uacpi_work_item *last = uacpi_work_queue;
        while (last->next) last = last->next;
        last->next = item;
    }
    uacpi_kernel_unlock_spinlock((uacpi_handle)1, flags);

    handler(ctx);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_wait_for_work_completion(void) {
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request *request) {
    console_printf("[uacpi] firmware request: type=%u\n", (unsigned)request->type);
    return UACPI_STATUS_OK;
}

int uacpi_port_init(void) {
    uacpi_status ret = uacpi_initialize(0);
    if (ret != UACPI_STATUS_OK) {
        console_printf("[uacpi] uacpi_initialize failed: %d\n", (int)ret);
        return -1;
    }
    console_write("[uacpi] table subsystem initialized\n");

    ret = uacpi_namespace_load();
    if (ret != UACPI_STATUS_OK) {
        console_printf("[uacpi] uacpi_namespace_load failed: %d\n", (int)ret);
        return -1;
    }
    console_write("[uacpi] namespace loaded\n");

    ret = uacpi_namespace_initialize();
    if (ret != UACPI_STATUS_OK) {
        console_printf("[uacpi] uacpi_namespace_initialize failed: %d\n", (int)ret);
        return -1;
    }
    console_write("[uacpi] namespace initialized\n");

    uacpi_port_initialized = true;
    return 0;
}

bool uacpi_port_is_ready(void) {
    return uacpi_port_initialized;
}
