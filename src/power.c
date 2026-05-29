#include "power.h"

#include "console.h"
#include "io.h"
#include "keyboard.h"
#include "pic.h"
#include "string.h"

#include <uacpi/uacpi.h>
#include <uacpi/sleep.h>

#define ACPI_PM1_SCI_EN      0x0001u
#define ACPI_PM1_SLEEP_SHIFT 10u
#define ACPI_PM1_SLEEP_MASK  0x1C00u
#define ACPI_PM1_SLEEP_EN    0x2000u

typedef struct PACKED {
    char signature[8];
    u8 checksum;
    char oem_id[6];
    u8 revision;
    u32 rsdt_address;
} rsdp_t;

typedef struct PACKED {
    char signature[4];
    u32 length;
    u8 revision;
    u8 checksum;
    char oem_id[6];
    char oem_table_id[8];
    char oem_revision[4];
    char creator_id[4];
    char creator_revision[4];
} acpi_sdt_header_t;

typedef struct PACKED {
    u8 address_space;
    u8 bit_width;
    u8 bit_offset;
    u8 access_size;
    u64 address;
} acpi_gas_t;

typedef struct PACKED {
    acpi_sdt_header_t header;
    u32 firmware_ctrl;
    u32 dsdt;
    u8 reserved1;
    u8 preferred_pm_profile;
    u16 sci_int;
    u32 smi_cmd;
    u8 acpi_enable;
    u8 acpi_disable;
    u8 s4bios_req;
    u8 pstate_cnt;
    u32 pm1a_evt_blk;
    u32 pm1b_evt_blk;
    u32 pm1a_cnt_blk;
    u32 pm1b_cnt_blk;
    u32 pm2_cnt_blk;
    u32 pm_tmr_blk;
    u32 gpe0_blk;
    u32 gpe1_blk;
    u8 pm1_evt_len;
    u8 pm1_cnt_len;
    u8 pm2_cnt_len;
    u8 pm_tmr_len;
    u8 gpe0_blk_len;
    u8 gpe1_blk_len;
    u8 gpe1_base;
    u8 cst_cnt;
    u16 p_lvl2_lat;
    u16 p_lvl3_lat;
    u16 flush_size;
    u16 flush_stride;
    u8 duty_offset;
    u8 duty_width;
    u8 day_alarm;
    u8 month_alarm;
    u8 century;
    u16 boot_architecture_flags;
    u8 reserved2;
    u32 flags;
    acpi_gas_t reset_reg;
    u8 reset_value;
    u8 reserved3[3];
    u64 x_firmware_ctrl;
    u64 x_dsdt;
    acpi_gas_t x_pm1a_evt_blk;
    acpi_gas_t x_pm1b_evt_blk;
    acpi_gas_t x_pm1a_cnt_blk;
    acpi_gas_t x_pm1b_cnt_blk;
    acpi_gas_t x_pm2_cnt_blk;
    acpi_gas_t x_pm_tmr_blk;
    acpi_gas_t x_gpe0_blk;
    acpi_gas_t x_gpe1_blk;
} fadt_t;

typedef struct PACKED {
    u16 limit;
    u32 base;
} idt_ptr_t;

typedef struct {
    bool acpi_ready;
    bool acpi_enabled;
    bool can_shutdown;
    bool can_suspend;
    bool can_reboot;
    bool used_emulator_fallbacks;
    u16 pm1a_evt_port;
    u16 pm1b_evt_port;
    u16 pm1a_cnt_port;
    u16 pm1b_cnt_port;
    u32 smi_cmd_port;
    u8 acpi_enable_value;
    u16 sleep_type_shutdown_a;
    u16 sleep_type_shutdown_b;
    u16 sleep_type_suspend_a;
    u16 sleep_type_suspend_b;
    acpi_gas_t reset_reg;
    u8 reset_value;
} power_state_t;

static power_state_t power_state;
static bool uacpi_available;

extern int uacpi_port_init(void);
extern bool uacpi_port_is_ready(void);

static bool checksum_ok(const void *table, u32 length) {
    const u8 *bytes = (const u8 *)table;
    u8 sum = 0;
    for (u32 i = 0; i < length; ++i) {
        sum = (u8)(sum + bytes[i]);
    }
    return sum == 0;
}

static const rsdp_t *scan_rsdp_range(u32 start, u32 end) {
    for (u32 addr = start; addr + sizeof(rsdp_t) <= end; addr += 16) {
        const rsdp_t *rsdp = (const rsdp_t *)(uintptr_t)addr;
        if (memcmp(rsdp->signature, "RSD PTR ", 8) == 0 && checksum_ok(rsdp, 20)) {
            return rsdp;
        }
    }
    return NULL;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
static const rsdp_t *find_rsdp(void) {
    u16 ebda_segment = *(volatile u16 *)(uintptr_t)0x40E;
    u32 ebda_base = (u32)ebda_segment << 4;
    const rsdp_t *rsdp = NULL;

    if (ebda_base >= 0x80000u && ebda_base < 0xA0000u) {
        rsdp = scan_rsdp_range(ebda_base, ebda_base + 1024u);
        if (rsdp) return rsdp;
    }
    return scan_rsdp_range(0xE0000u, 0x100000u);
}
#pragma GCC diagnostic pop

static const acpi_sdt_header_t *acpi_find_table(const acpi_sdt_header_t *rsdt, const char *signature) {
    const u32 *entries;
    u32 count;
    if (!rsdt || rsdt->length < sizeof(acpi_sdt_header_t)) return NULL;
    entries = (const u32 *)(const void *)((const u8 *)rsdt + sizeof(acpi_sdt_header_t));
    count = (rsdt->length - sizeof(acpi_sdt_header_t)) / sizeof(u32);
    for (u32 i = 0; i < count; ++i) {
        const acpi_sdt_header_t *table = (const acpi_sdt_header_t *)(uintptr_t)entries[i];
        if (!table || !checksum_ok(table, table->length)) continue;
        if (memcmp(table->signature, signature, 4) == 0) return table;
    }
    return NULL;
}

static bool aml_read_pkg_length(const u8 *aml, const u8 *end, u32 *value, u32 *used) {
    u8 lead;
    u32 bytes_follow;
    u32 result;
    if (aml >= end) return false;
    lead = aml[0];
    if ((lead & 0xC0) == 0) {
        *value = lead & 0x3F;
        *used = 1;
        return true;
    }
    bytes_follow = (lead >> 4) & 0x3;
    result = lead & 0x0F;
    *used = 1;
    for (u32 i = 0; i < bytes_follow; ++i) {
        if (aml + *used >= end) return false;
        result |= ((u32)aml[*used]) << (8 * i + 4);
        *used += 1;
    }
    *value = result;
    return true;
}

static bool aml_read_integer(const u8 *aml, const u8 *end, u64 *value) {
    u32 pkg_len, pkg_used;
    if (aml >= end) return false;
    switch (aml[0]) {
    case 0x00: *value = 0; return true;
    case 0x01: *value = 1; return true;
    case 0x60: *value = 0xFFFFFFFFu; return true;
    case 0x5Bu: return false;
    default: break;
    }
    if (aml[0] >= 0x0A && aml[0] <= 0x0D) {
        u32 len = 1u << (aml[0] - 0x0A);
        *value = 0;
        for (u32 i = 0; i < len; ++i) {
            if (aml + 1 + i >= end) return false;
            *value |= ((u64)aml[1 + i]) << (i * 8);
        }
        return true;
    }
    if (aml[0] == 0x5B && aml + 1 < end && aml[1] == 0x30) {
        *value = 0;
        for (u32 i = 0; i < 8; ++i) {
            if (aml + 2 + i >= end) return false;
            *value |= ((u64)aml[2 + i]) << (i * 8);
        }
        return true;
    }
    return false;
}

static bool aml_extract_sleep_type(const acpi_sdt_header_t *dsdt, const char *name, u16 *type_a, u16 *type_b) {
    const u8 *aml = (const u8 *)dsdt;
    const u8 *end = aml + dsdt->length;
    u32 offset = sizeof(acpi_sdt_header_t);
    while (offset + 5 < dsdt->length) {
        if (aml[offset] == 0x08 && aml[offset + 1] == '_' &&
            aml[offset + 2] == name[1] && aml[offset + 3] == name[2] &&
            aml[offset + 4] == name[3]) {
            offset += 5;
            u64 val_a, val_b;
            u32 pkg_len, pkg_used;
            if (aml + offset < end && aml[offset] == 0x12) {
                offset++;
                if (!aml_read_pkg_length(aml + offset, end, &pkg_len, &pkg_used)) return false;
                offset += pkg_used;
                if (!aml_read_integer(aml + offset, end, &val_a)) return false;
                while (aml[offset] != 0x0A && aml[offset] != 0x0B &&
                       aml[offset] != 0x0C && aml[offset] != 0x0D &&
                       aml[offset] != 0x00 && aml[offset] != 0x01) offset++;
                if (!aml_read_integer(aml + offset, end, &val_a)) return false;
                offset++;
                if (!aml_read_integer(aml + offset, end, &val_b)) return false;
                *type_a = (u16)(val_a & 7);
                *type_b = (u16)(val_b & 7);
                return true;
            }
        }
        offset++;
    }
    return false;
}

static bool acpi_enable_if_needed(void) {
    if (power_state.acpi_enabled) return true;
    if (power_state.smi_cmd_port == 0 || power_state.acpi_enable_value == 0) return false;
    if (!power_state.acpi_ready) return false;
    outb((u16)power_state.smi_cmd_port, power_state.acpi_enable_value);
    for (u32 timeout = 0; timeout < 1000; ++timeout) {
        io_wait();
        if ((inw(power_state.pm1a_cnt_port) & ACPI_PM1_SCI_EN) != 0) {
            power_state.acpi_enabled = true;
            return true;
        }
    }
    return false;
}

static void clear_pm1_events(void) {
    if (power_state.pm1a_evt_port) outw(power_state.pm1a_evt_port, 0xFFFF);
    if (power_state.pm1b_evt_port) outw(power_state.pm1b_evt_port, 0xFFFF);
}

static bool acpi_enter_sleep(u16 slp_typa, u16 slp_typb) {
    if (!acpi_enable_if_needed()) return false;
    clear_pm1_events();
    u16 pm1a_cnt = inw(power_state.pm1a_cnt_port);
    pm1a_cnt = (u16)((pm1a_cnt & ~ACPI_PM1_SLEEP_MASK) | (slp_typa << ACPI_PM1_SLEEP_SHIFT) | ACPI_PM1_SLEEP_EN);
    if (power_state.pm1b_cnt_port) {
        u16 pm1b_cnt = inw(power_state.pm1b_cnt_port);
        pm1b_cnt = (u16)((pm1b_cnt & ~ACPI_PM1_SLEEP_MASK) | (slp_typb << ACPI_PM1_SLEEP_SHIFT) | ACPI_PM1_SLEEP_EN);
        outw(power_state.pm1b_cnt_port, pm1b_cnt);
    }
    outw(power_state.pm1a_cnt_port, pm1a_cnt);
    for (u32 spin = 0; spin < 100000u; ++spin) io_wait();
    return true;
}

static void qemu_shutdown_fallback(void) {
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outl(0x4004, 0x3400);
}

static void keyboard_controller_reboot(void) {
    while (inb(0x64) & 0x02);
    outb(0x64, 0xFE);
}

void power_init(void) {
    memset(&power_state, 0, sizeof(power_state));
    power_state.can_reboot = true;

    uacpi_available = (uacpi_port_init() == 0);

    if (uacpi_available) {
        console_write("[power] uACPI is available\n");
        power_state.can_shutdown = true;
        power_state.can_suspend = true;
        power_state.acpi_ready = true;
        power_state.acpi_enabled = true;
        return;
    }

    const rsdp_t *rsdp;
    const acpi_sdt_header_t *rsdt;
    const fadt_t *fadt;
    const acpi_sdt_header_t *dsdt;

    rsdp = find_rsdp();
    if (!rsdp) {
        power_state.used_emulator_fallbacks = true;
        return;
    }

    rsdt = (const acpi_sdt_header_t *)(uintptr_t)rsdp->rsdt_address;
    if (!rsdt || memcmp(rsdt->signature, "RSDT", 4) != 0 || !checksum_ok(rsdt, rsdt->length)) {
        power_state.used_emulator_fallbacks = true;
        return;
    }

    fadt = (const fadt_t *)acpi_find_table(rsdt, "FACP");
    if (!fadt || fadt->header.length < sizeof(acpi_sdt_header_t) + 54u) {
        power_state.used_emulator_fallbacks = true;
        return;
    }

    dsdt = (const acpi_sdt_header_t *)(uintptr_t)fadt->dsdt;
    if (!dsdt || memcmp(dsdt->signature, "DSDT", 4) != 0 || !checksum_ok(dsdt, dsdt->length)) {
        power_state.used_emulator_fallbacks = true;
        return;
    }

    power_state.pm1a_evt_port = (u16)fadt->pm1a_evt_blk;
    power_state.pm1b_evt_port = (u16)fadt->pm1b_evt_blk;
    power_state.pm1a_cnt_port = (u16)fadt->pm1a_cnt_blk;
    power_state.pm1b_cnt_port = (u16)fadt->pm1b_cnt_blk;
    power_state.smi_cmd_port = fadt->smi_cmd;
    power_state.acpi_enable_value = fadt->acpi_enable;
    if (fadt->header.length >= 129u) {
        power_state.reset_reg = fadt->reset_reg;
        power_state.reset_value = fadt->reset_value;
    }
    power_state.acpi_ready = power_state.pm1a_cnt_port != 0;
    power_state.acpi_enabled = power_state.acpi_ready && ((inw(power_state.pm1a_cnt_port) & ACPI_PM1_SCI_EN) != 0);
    power_state.can_shutdown =
        aml_extract_sleep_type(dsdt, "_S5_", &power_state.sleep_type_shutdown_a, &power_state.sleep_type_shutdown_b);
    power_state.can_suspend =
        aml_extract_sleep_type(dsdt, "_S1_", &power_state.sleep_type_suspend_a, &power_state.sleep_type_suspend_b);
}

power_info_t power_info(void) {
    power_info_t info;
    if (uacpi_available) {
        info.acpi_ready = true;
        info.can_shutdown = true;
        info.can_suspend = true;
        info.can_reboot = true;
        info.suspend_uses_fallback = false;
        info.used_emulator_fallbacks = false;
        return info;
    }
    info.acpi_ready = power_state.acpi_ready;
    info.can_shutdown = power_state.can_shutdown || power_state.used_emulator_fallbacks;
    info.can_suspend = true;
    info.can_reboot = power_state.can_reboot;
    info.suspend_uses_fallback = !power_state.can_suspend;
    info.used_emulator_fallbacks = power_state.used_emulator_fallbacks;
    return info;
}

bool power_shutdown(void) {
    if (uacpi_available) {
        uacpi_status st;
        st = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S5);
        if (st == UACPI_STATUS_OK)
            st = uacpi_enter_sleep_state(UACPI_SLEEP_STATE_S5);
        if (st != UACPI_STATUS_OK)
            console_printf("[power] uACPI S5 failed (%d), trying fallback\n", (int)st);
    }

    if (power_state.can_shutdown &&
        acpi_enter_sleep(power_state.sleep_type_shutdown_a, power_state.sleep_type_shutdown_b)) {
        for (u32 spin = 0; spin < 1000000u; ++spin) {}
    }

    qemu_shutdown_fallback();
    for (u32 spin = 0; spin < 1000000u; ++spin) {}
    return false;
}

bool power_suspend(void) {
    if (uacpi_available) {
        uacpi_status st;
        st = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S3);
        if (st != UACPI_STATUS_OK)
            st = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S1);
        if (st == UACPI_STATUS_OK)
            st = uacpi_enter_sleep_state(UACPI_SLEEP_STATE_S1);
        if (st == UACPI_STATUS_OK) return true;
        console_printf("[power] uACPI S1 failed (%d), trying fallback\n", (int)st);
    }

    if (power_state.can_suspend &&
        acpi_enter_sleep(power_state.sleep_type_suspend_a, power_state.sleep_type_suspend_b)) {
        return true;
    }

    console_write("ACPI S1 is unavailable. Falling back to interrupt-driven idle wait.\n");
    while (!keyboard_has_input()) {
        cpu_halt();
    }
    return false;
}

NORETURN void power_reboot(void) {
    if (uacpi_available) {
        uacpi_status st = uacpi_reboot();
        if (st == UACPI_STATUS_OK) {
            for (;;) cpu_halt();
        }
        console_printf("[power] uACPI reboot failed (%d), trying fallback\n", (int)st);
    }

    if (power_state.reset_reg.address_space == 1 &&
        power_state.reset_reg.address != 0 &&
        power_state.reset_reg.address <= 0xFFFFu) {
        outb((u16)power_state.reset_reg.address, power_state.reset_value);
    }

    keyboard_controller_reboot();

    outb(0xCF9, 0x02);
    outb(0xCF9, 0x06);

    {
        static const idt_ptr_t empty_idt = {0, 0};
        __asm__ volatile ("lidt (%0)" : : "r"(&empty_idt) : "memory");
        __asm__ volatile ("int3");
    }

    for (;;) cpu_halt();
}
