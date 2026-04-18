#include "power.h"

#include "console.h"
#include "io.h"
#include "keyboard.h"
#include "pic.h"
#include "string.h"

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
    u32 oem_revision;
    u32 creator_id;
    u32 creator_revision;
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
    u8 reserved0;
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
    u8 day_alrm;
    u8 mon_alrm;
    u8 century;
    u16 iapc_boot_arch;
    u8 reserved1;
    u32 flags;
    acpi_gas_t reset_reg;
    u8 reset_value;
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
    u8 sleep_type_shutdown_a;
    u8 sleep_type_shutdown_b;
    u8 sleep_type_suspend_a;
    u8 sleep_type_suspend_b;
    acpi_gas_t reset_reg;
    u8 reset_value;
} power_state_t;

static power_state_t power_state;
static const volatile u16 *const bios_data_area = (const volatile u16 *)(uintptr_t)0x400;

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
    u16 ebda_segment = bios_data_area[0x0E / 2];
    u32 ebda_base = (u32)ebda_segment << 4;
    const rsdp_t *rsdp = NULL;

    if (ebda_base >= 0x80000u && ebda_base < 0xA0000u) {
        rsdp = scan_rsdp_range(ebda_base, ebda_base + 1024u);
        if (rsdp) {
            return rsdp;
        }
    }

    return scan_rsdp_range(0xE0000u, 0x100000u);
}
#pragma GCC diagnostic pop

static const acpi_sdt_header_t *acpi_find_table(const acpi_sdt_header_t *rsdt, const char *signature) {
    const u32 *entries;
    u32 count;

    if (!rsdt || rsdt->length < sizeof(acpi_sdt_header_t)) {
        return NULL;
    }

    entries = (const u32 *)(const void *)((const u8 *)rsdt + sizeof(acpi_sdt_header_t));
    count = (rsdt->length - sizeof(acpi_sdt_header_t)) / sizeof(u32);

    for (u32 i = 0; i < count; ++i) {
        const acpi_sdt_header_t *table = (const acpi_sdt_header_t *)(uintptr_t)entries[i];
        if (!table || !checksum_ok(table, table->length)) {
            continue;
        }
        if (memcmp(table->signature, signature, 4) == 0) {
            return table;
        }
    }

    return NULL;
}

static bool aml_read_pkg_length(const u8 *aml, const u8 *end, u32 *value, u32 *used) {
    u8 lead;
    u32 bytes_follow;
    u32 result;

    if (aml >= end) {
        return false;
    }

    lead = aml[0];
    bytes_follow = (u32)((lead >> 6) & 0x3u);
    if (aml + 1 + bytes_follow > end) {
        return false;
    }

    result = lead & 0x0Fu;
    for (u32 i = 0; i < bytes_follow; ++i) {
        result |= (u32)aml[1 + i] << (4 + i * 8);
    }

    *value = result;
    *used = 1 + bytes_follow;
    return true;
}

static bool aml_read_integer(const u8 *aml, const u8 *end, u32 *value, u32 *used) {
    if (aml >= end) {
        return false;
    }

    switch (aml[0]) {
        case 0x00:
            *value = 0;
            *used = 1;
            return true;
        case 0x01:
            *value = 1;
            *used = 1;
            return true;
        case 0x0A:
            if (aml + 2 > end) {
                return false;
            }
            *value = aml[1];
            *used = 2;
            return true;
        case 0x0B:
            if (aml + 3 > end) {
                return false;
            }
            *value = (u32)aml[1] | ((u32)aml[2] << 8);
            *used = 3;
            return true;
        case 0x0C:
            if (aml + 5 > end) {
                return false;
            }
            *value = (u32)aml[1] |
                     ((u32)aml[2] << 8) |
                     ((u32)aml[3] << 16) |
                     ((u32)aml[4] << 24);
            *used = 5;
            return true;
        default:
            return false;
    }
}

static bool aml_extract_sleep_type(const acpi_sdt_header_t *dsdt,
                                   const char *name,
                                   u8 *type_a,
                                   u8 *type_b) {
    const u8 *aml = (const u8 *)dsdt + sizeof(acpi_sdt_header_t);
    const u8 *end = (const u8 *)dsdt + dsdt->length;

    for (const u8 *cursor = aml; cursor + 6 < end; ++cursor) {
        bool name_match = memcmp(cursor, name, 4) == 0;
        bool rooted_name = cursor > aml && (cursor[-1] == '\\' || cursor[-1] == '^') && memcmp(cursor, name, 4) == 0;
        bool preceded_by_nameop =
            (cursor > aml && cursor[-1] == 0x08) ||
            (cursor > aml + 1 && cursor[-2] == 0x08 && (cursor[-1] == '\\' || cursor[-1] == '^'));

        if ((!name_match && !rooted_name) || !preceded_by_nameop) {
            continue;
        }

        cursor += 4;
        if (cursor >= end || *cursor != 0x12) {
            continue;
        }

        {
            u32 pkg_length;
            u32 consumed;
            u32 value_a;
            u32 value_b;

            cursor++;
            if (!aml_read_pkg_length(cursor, end, &pkg_length, &consumed)) {
                continue;
            }
            cursor += consumed;

            if (cursor >= end) {
                continue;
            }

            /* The first byte is the package element count. After that the real payload starts. */
            cursor++;
            if (!aml_read_integer(cursor, end, &value_a, &consumed)) {
                continue;
            }
            cursor += consumed;
            if (!aml_read_integer(cursor, end, &value_b, &consumed)) {
                continue;
            }

            *type_a = (u8)(value_a & 0x7u);
            *type_b = (u8)(value_b & 0x7u);
            return true;
        }
    }

    return false;
}

static bool acpi_enable_if_needed(void) {
    if (!power_state.pm1a_cnt_port) {
        return false;
    }

    if ((inw(power_state.pm1a_cnt_port) & ACPI_PM1_SCI_EN) != 0) {
        power_state.acpi_enabled = true;
        return true;
    }

    if (!power_state.smi_cmd_port || !power_state.acpi_enable_value || power_state.smi_cmd_port > 0xFFFFu) {
        return false;
    }

    outb((u16)power_state.smi_cmd_port, power_state.acpi_enable_value);

    for (u32 spin = 0; spin < 1000000u; ++spin) {
        if ((inw(power_state.pm1a_cnt_port) & ACPI_PM1_SCI_EN) != 0) {
            power_state.acpi_enabled = true;
            return true;
        }
    }

    return false;
}

static void clear_pm1_events(void) {
    if (power_state.pm1a_evt_port) {
        outw(power_state.pm1a_evt_port, 0xFFFFu);
    }
    if (power_state.pm1b_evt_port) {
        outw(power_state.pm1b_evt_port, 0xFFFFu);
    }
}

static bool acpi_enter_sleep(u8 type_a, u8 type_b) {
    u16 value_a;
    u16 value_b = 0;

    if (!power_state.acpi_ready || !power_state.pm1a_cnt_port) {
        return false;
    }

    if (!power_state.acpi_enabled && !acpi_enable_if_needed()) {
        return false;
    }

    clear_pm1_events();

    value_a = (u16)(inw(power_state.pm1a_cnt_port) & ~(ACPI_PM1_SLEEP_MASK | ACPI_PM1_SLEEP_EN));
    outw(power_state.pm1a_cnt_port, (u16)(value_a | ((u16)type_a << ACPI_PM1_SLEEP_SHIFT)));

    if (power_state.pm1b_cnt_port) {
        value_b = (u16)(inw(power_state.pm1b_cnt_port) & ~(ACPI_PM1_SLEEP_MASK | ACPI_PM1_SLEEP_EN));
        outw(power_state.pm1b_cnt_port, (u16)(value_b | ((u16)type_b << ACPI_PM1_SLEEP_SHIFT)));
    }

    outw(power_state.pm1a_cnt_port,
         (u16)(value_a | ((u16)type_a << ACPI_PM1_SLEEP_SHIFT) | ACPI_PM1_SLEEP_EN));
    if (power_state.pm1b_cnt_port) {
        outw(power_state.pm1b_cnt_port,
             (u16)(value_b | ((u16)type_b << ACPI_PM1_SLEEP_SHIFT) | ACPI_PM1_SLEEP_EN));
    }

    return true;
}

static void qemu_shutdown_fallback(void) {
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);
}

static void keyboard_controller_reboot(void) {
    for (u32 spin = 0; spin < 0x10000u; ++spin) {
        if ((inb(0x64) & 0x02u) == 0) {
            outb(0x64, 0xFE);
            return;
        }
    }
}

void power_init(void) {
    const rsdp_t *rsdp;
    const acpi_sdt_header_t *rsdt;
    const fadt_t *fadt;
    const acpi_sdt_header_t *dsdt;

    memset(&power_state, 0, sizeof(power_state));
    power_state.can_reboot = true;

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

    info.acpi_ready = power_state.acpi_ready;
    info.can_shutdown = power_state.can_shutdown || power_state.used_emulator_fallbacks;
    info.can_suspend = true;
    info.can_reboot = power_state.can_reboot;
    info.suspend_uses_fallback = !power_state.can_suspend;
    info.used_emulator_fallbacks = power_state.used_emulator_fallbacks;
    return info;
}

bool power_shutdown(void) {
    if (power_state.can_shutdown &&
        acpi_enter_sleep(power_state.sleep_type_shutdown_a, power_state.sleep_type_shutdown_b)) {
        for (u32 spin = 0; spin < 1000000u; ++spin) {
        }
    }

    qemu_shutdown_fallback();

    for (u32 spin = 0; spin < 1000000u; ++spin) {
    }
    return false;
}

bool power_suspend(void) {
    if (power_state.can_suspend &&
        acpi_enter_sleep(power_state.sleep_type_suspend_a, power_state.sleep_type_suspend_b)) {
        return true;
    }

    /* If firmware doesn't give us S1, we still offer a quiet idle instead of pretending nothing happened. */
    console_write("ACPI S1 is unavailable here. Falling back to an interrupt-driven idle wait.\n");
    while (!keyboard_has_input()) {
        cpu_halt();
    }
    return false;
}

NORETURN void power_reboot(void) {
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

    for (;;) {
        cpu_halt();
    }
}
