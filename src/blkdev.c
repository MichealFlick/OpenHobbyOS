#include "blkdev.h"

#include "console.h"
#include "string.h"

static blkdev_t devices[BLKDEV_MAX_DEVICES];

void blkdev_init(void) {
    memset(devices, 0, sizeof(devices));
    for (u32 i = 0; i < BLKDEV_MAX_DEVICES; i++) {
        devices[i].id = i;
        devices[i].sector_size = BLKDEV_SECTOR_SIZE;
    }
}

bool blkdev_present(u32 id) {
    if (id >= BLKDEV_MAX_DEVICES) {
        return false;
    }
    return devices[id].present;
}

u64 blkdev_total_sectors(u32 id) {
    if (id >= BLKDEV_MAX_DEVICES || !devices[id].present) {
        return 0;
    }
    return devices[id].total_sectors;
}

blkdev_t *blkdev_get(u32 id) {
    if (id >= BLKDEV_MAX_DEVICES || !devices[id].present) {
        return NULL;
    }
    return &devices[id];
}

int blkdev_read(u32 id, u64 lba, u32 count, void *buffer) {
    blkdev_t *dev = blkdev_get(id);
    if (!dev || !dev->ops || !dev->ops->read) {
        return -1;
    }
    if (lba + count > dev->total_sectors) {
        return -1;
    }
    return dev->ops->read(dev, (u32)lba, count, buffer);
}

int blkdev_write(u32 id, u64 lba, u32 count, const void *buffer) {
    blkdev_t *dev = blkdev_get(id);
    if (!dev || !dev->ops || !dev->ops->write) {
        return -1;
    }
    if (lba + count > dev->total_sectors) {
        return -1;
    }
    return dev->ops->write(dev, (u32)lba, count, buffer);
}

/* Internal: register a device (called by ATA driver) */
void blkdev_register(u32 id, u64 total_sectors, const blkdev_ops_t *ops, void *private) {
    if (id >= BLKDEV_MAX_DEVICES) {
        return;
    }
    devices[id].present = true;
    devices[id].total_sectors = total_sectors;
    devices[id].ops = ops;
    devices[id].private = private;
}
