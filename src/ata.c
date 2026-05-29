#include "ata.h"

#include "console.h"
#include "io.h"
#include "memory.h"
#include "panic.h"
#include "string.h"

#define ATA_MAX_DEVICES 4
#define ATA_TIMEOUT 100000

typedef struct {
    u16 io_base;
    u16 ctrl_base;
    u8 drive;
    bool present;
    u64 total_sectors;
} ata_device_t;

static ata_device_t ata_devices[ATA_MAX_DEVICES];

static inline u8 ata_read_reg(u16 io_base, u8 reg) {
    return inb(io_base + reg);
}

static inline void ata_write_reg(u16 io_base, u8 reg, u8 value) {
    outb(io_base + reg, value);
}

static void ata_delay(u16 io_base) {
    ata_read_reg(io_base, ATA_REG_STATUS);
    ata_read_reg(io_base, ATA_REG_STATUS);
    ata_read_reg(io_base, ATA_REG_STATUS);
    ata_read_reg(io_base, ATA_REG_STATUS);
}

static void ata_select_drive(u16 io_base, u8 drive) {
    ata_write_reg(io_base, ATA_REG_DRIVE, 0xA0 | (drive << 4));
    ata_delay(io_base);
}

static bool ata_wait_ready(u16 io_base, bool check_drq) {
    u32 timeout = ATA_TIMEOUT;
    while (timeout--) {
        u8 status = ata_read_reg(io_base, ATA_REG_STATUS);
        if (status & ATA_STATUS_ERR) {
            return false;
        }
        if (status & ATA_STATUS_BSY) {
            continue;
        }
        if (!check_drq || (status & ATA_STATUS_DRQ)) {
            return true;
        }
    }
    return false;
}

static int ata_read_sectors(ata_device_t *dev, u32 lba, u32 count, void *buffer) {
    u8 *buf = buffer;
    
    for (u32 i = 0; i < count; i++) {
        ata_select_drive(dev->io_base, dev->drive);
        
        ata_write_reg(dev->io_base, ATA_REG_FEATURES, 0);
        ata_write_reg(dev->io_base, ATA_REG_SECTOR_COUNT, 1);
        ata_write_reg(dev->io_base, ATA_REG_LBA_LOW, (lba + i) & 0xFF);
        ata_write_reg(dev->io_base, ATA_REG_LBA_MID, ((lba + i) >> 8) & 0xFF);
        ata_write_reg(dev->io_base, ATA_REG_LBA_HIGH, ((lba + i) >> 16) & 0xFF);
        ata_write_reg(dev->io_base, ATA_REG_DRIVE, 0xE0 | (dev->drive << 4) | ((lba + i) >> 24));
        
        ata_write_reg(dev->io_base, ATA_REG_COMMAND, ATA_CMD_READ_SECTORS);
        
        if (!ata_wait_ready(dev->io_base, true)) {
            return -1;
        }
        
        for (u32 j = 0; j < 256; j++) {
            u16 data = inw(dev->io_base + ATA_REG_DATA);
            buf[j * 2] = data & 0xFF;
            buf[j * 2 + 1] = (data >> 8) & 0xFF;
        }
        
        buf += 512;
    }
    
    return 0;
}

static int ata_write_sectors(ata_device_t *dev, u32 lba, u32 count, const void *buffer) {
    const u8 *buf = buffer;
    
    for (u32 i = 0; i < count; i++) {
        ata_select_drive(dev->io_base, dev->drive);
        
        ata_write_reg(dev->io_base, ATA_REG_FEATURES, 0);
        ata_write_reg(dev->io_base, ATA_REG_SECTOR_COUNT, 1);
        ata_write_reg(dev->io_base, ATA_REG_LBA_LOW, (lba + i) & 0xFF);
        ata_write_reg(dev->io_base, ATA_REG_LBA_MID, ((lba + i) >> 8) & 0xFF);
        ata_write_reg(dev->io_base, ATA_REG_LBA_HIGH, ((lba + i) >> 16) & 0xFF);
        ata_write_reg(dev->io_base, ATA_REG_DRIVE, 0xE0 | (dev->drive << 4) | ((lba + i) >> 24));
        
        ata_write_reg(dev->io_base, ATA_REG_COMMAND, ATA_CMD_WRITE_SECTORS);
        
        if (!ata_wait_ready(dev->io_base, true)) {
            return -1;
        }
        
        for (u32 j = 0; j < 256; j++) {
            u16 data = buf[j * 2] | (buf[j * 2 + 1] << 8);
            outw(dev->io_base + ATA_REG_DATA, data);
        }
        
        buf += 512;
    }
    
    return 0;
}

static int ata_identify(ata_device_t *dev) {
    ata_select_drive(dev->io_base, dev->drive);
    
    ata_write_reg(dev->io_base, ATA_REG_SECTOR_COUNT, 0);
    ata_write_reg(dev->io_base, ATA_REG_LBA_LOW, 0);
    ata_write_reg(dev->io_base, ATA_REG_LBA_MID, 0);
    ata_write_reg(dev->io_base, ATA_REG_LBA_HIGH, 0);
    ata_write_reg(dev->io_base, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    
    u8 status = ata_read_reg(dev->io_base, ATA_REG_STATUS);
    if (status == 0) {
        return -1;
    }
    
    u32 timeout = ATA_TIMEOUT;
    while (timeout--) {
        status = ata_read_reg(dev->io_base, ATA_REG_STATUS);
        if (status & ATA_STATUS_ERR) {
            return -1;
        }
        if (!(status & ATA_STATUS_BSY) && (status & ATA_STATUS_DRQ)) {
            break;
        }
    }
    
    if (timeout == 0) {
        return -1;
    }
    
    u16 identify_data[256];
    for (int i = 0; i < 256; i++) {
        identify_data[i] = inw(dev->io_base + ATA_REG_DATA);
    }
    
    u64 total_sectors = identify_data[60] | ((u64)identify_data[61] << 16);
    if (identify_data[83] & 0x0400) {
        total_sectors = identify_data[100] | 
                       ((u64)identify_data[101] << 16) |
                       ((u64)identify_data[102] << 32) |
                       ((u64)identify_data[103] << 48);
    }
    
    dev->total_sectors = total_sectors;
    dev->present = true;
    
    return 0;
}

static int ata_blkdev_read(blkdev_t *dev, u32 lba, u32 count, void *buffer) {
    ata_device_t *ata_dev = (ata_device_t *)dev->private;
    return ata_read_sectors(ata_dev, lba, count, buffer);
}

static int ata_blkdev_write(blkdev_t *dev, u32 lba, u32 count, const void *buffer) {
    ata_device_t *ata_dev = (ata_device_t *)dev->private;
    return ata_write_sectors(ata_dev, lba, count, buffer);
}

static const blkdev_ops_t ata_blkdev_ops = {
    .read = ata_blkdev_read,
    .write = ata_blkdev_write,
};

void ata_init(void) {
    memset(ata_devices, 0, sizeof(ata_devices));
    
    ata_devices[0].io_base = ATA_PRIMARY_IO;
    ata_devices[0].ctrl_base = ATA_PRIMARY_CTRL;
    ata_devices[0].drive = 0;
    
    ata_devices[1].io_base = ATA_PRIMARY_IO;
    ata_devices[1].ctrl_base = ATA_PRIMARY_CTRL;
    ata_devices[1].drive = 1;
    
    ata_devices[2].io_base = ATA_SECONDARY_IO;
    ata_devices[2].ctrl_base = ATA_SECONDARY_CTRL;
    ata_devices[2].drive = 0;
    
    ata_devices[3].io_base = ATA_SECONDARY_IO;
    ata_devices[3].ctrl_base = ATA_SECONDARY_CTRL;
    ata_devices[3].drive = 1;
    
    u32 detected = 0;
    for (int i = 0; i < ATA_MAX_DEVICES; i++) {
        if (ata_identify(&ata_devices[i]) == 0) {
            blkdev_register(i, ata_devices[i].total_sectors, &ata_blkdev_ops, &ata_devices[i]);
            console_printf("[ata] hda%c: %u sectors (%u MB)\n",
                        'a' + i,
                        (u32)ata_devices[i].total_sectors,
                        (u32)(ata_devices[i].total_sectors / 2048));
            detected++;
        }
    }
    
    if (detected == 0) {
        console_printf("[ata] no drives detected\n");
    }
}
