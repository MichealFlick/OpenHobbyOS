#include "pci.h"
#include "console.h"
#include "io.h"

#define MAX_BUSES 1
#define MAX_SLOTS 32
#define MAX_FUNCS 8

static pci_device_t devices[256];
static u32 device_count;

u32 pci_config_read(u8 bus, u8 slot, u8 func, u8 offset) {
    u32 address = ((u32)bus << 16) | ((u32)slot << 11) | ((u32)func << 8) | (offset & 0xFC) | 0x80000000u;
    outl(PCI_CONF_ADDR, address);
    return inl(PCI_CONF_DATA);
}

void pci_config_write(u8 bus, u8 slot, u8 func, u8 offset, u32 value) {
    u32 address = ((u32)bus << 16) | ((u32)slot << 11) | ((u32)func << 8) | (offset & 0xFC) | 0x80000000u;
    outl(PCI_CONF_ADDR, address);
    outl(PCI_CONF_DATA, value);
}

static u32 pci_read_bar(u8 bus, u8 slot, u8 func, u8 barnum) {
    return pci_config_read(bus, slot, func, 0x10 + barnum * 4);
}

void pci_scan(void) {
    device_count = 0;
    for (u8 bus = 0; bus < MAX_BUSES; bus++) {
        for (u8 slot = 0; slot < MAX_SLOTS; slot++) {
            for (u8 func = 0; func < MAX_FUNCS; func++) {
                u32 id = pci_config_read(bus, slot, func, 0);
                u16 vendor = (u16)(id & 0xFFFF);
                if (vendor == PCI_VENDOR_NONE) {
                    if (func == 0) break;
                    continue;
                }
                u16 device = (u16)(id >> 16);
                u32 class_reg = pci_config_read(bus, slot, func, 8);
                u32 header = pci_config_read(bus, slot, func, 0x0C);
                u32 irq_reg = pci_config_read(bus, slot, func, 0x3C);
                pci_device_t *d = &devices[device_count++];
                d->vendor_id = vendor;
                d->device_id = device;
                d->bus = bus;
                d->slot = slot;
                d->func = func;
                d->class_code = (u8)(class_reg >> 24);
                d->subclass = (u8)((class_reg >> 16) & 0xFF);
                d->prog_if = (u8)((class_reg >> 8) & 0xFF);
                d->irq = (u8)(irq_reg & 0xFF);
                for (int b = 0; b < 6; b++) {
                    u32 bar = pci_read_bar(bus, slot, func, b);
                    *(&d->bar0 + b) = bar;
                    if (bar & 1) {
                        inl(bar & 0xFFFC);
                    }
                }
                if (func == 0 && (header & 0x80)) continue;
                if (func == 0 && !(header & 0x80)) break;
            }
        }
    }
    console_printf("[pci] scanned %u devices\n", device_count);
    for (u32 i = 0; i < device_count; i++) {
        const pci_device_t *d = &devices[i];
        console_printf("[pci] %02x:%02x.%x %04x:%04x class=%02x subclass=%02x irq=%u bar0=%x\n",
                       d->bus, d->slot, d->func, d->vendor_id, d->device_id,
                       d->class_code, d->subclass, d->irq, d->bar0);
    }
}

u32 pci_device_count(void) { return device_count; }

const pci_device_t *pci_device(u32 index) {
    return index < device_count ? &devices[index] : NULL;
}

const pci_device_t *pci_find(u16 vendor, u16 device) {
    for (u32 i = 0; i < device_count; i++) {
        if (devices[i].vendor_id == vendor && devices[i].device_id == device)
            return &devices[i];
    }
    return NULL;
}

const pci_device_t *pci_find_by_class(u8 class, u8 subclass) {
    for (u32 i = 0; i < device_count; i++) {
        if (devices[i].class_code == class && devices[i].subclass == subclass)
            return &devices[i];
    }
    return NULL;
}
