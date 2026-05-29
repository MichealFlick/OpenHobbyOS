#ifndef OHOS_PCI_H
#define OHOS_PCI_H

#include "types.h"

#define PCI_CONF_ADDR 0xCF8
#define PCI_CONF_DATA 0xCFC

#define PCI_VENDOR_NONE 0xFFFF

#define PCI_CLASS_NETWORK 0x02
#define PCI_SUBCLASS_ETHERNET 0x00

typedef struct {
    u16 vendor_id;
    u16 device_id;
    u8 bus;
    u8 slot;
    u8 func;
    u8 class_code;
    u8 subclass;
    u8 prog_if;
    u8 irq;
    u32 bar0;
    u32 bar1;
    u32 bar2;
    u32 bar3;
    u32 bar4;
    u32 bar5;
} pci_device_t;

u32 pci_config_read(u8 bus, u8 slot, u8 func, u8 offset);
void pci_config_write(u8 bus, u8 slot, u8 func, u8 offset, u32 value);
void pci_scan(void);
u32 pci_device_count(void);
const pci_device_t *pci_device(u32 index);
const pci_device_t *pci_find(u16 vendor, u16 device);
const pci_device_t *pci_find_by_class(u8 class, u8 subclass);

#endif
