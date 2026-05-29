#ifndef OHOS_BLKDEV_H
#define OHOS_BLKDEV_H

#include "types.h"

#define BLKDEV_SECTOR_SIZE 512
#define BLKDEV_MAX_DEVICES 4

typedef struct blkdev blkdev_t;

typedef struct {
    int (*read)(blkdev_t *dev, u32 lba, u32 count, void *buffer);
    int (*write)(blkdev_t *dev, u32 lba, u32 count, const void *buffer);
} blkdev_ops_t;

struct blkdev {
    u32 id;
    u64 total_sectors;
    u32 sector_size;
    const blkdev_ops_t *ops;
    void *private;
    bool present;
};

void blkdev_init(void);
bool blkdev_present(u32 id);
u64 blkdev_total_sectors(u32 id);
blkdev_t *blkdev_get(u32 id);
int blkdev_read(u32 id, u64 lba, u32 count, void *buffer);
int blkdev_write(u32 id, u64 lba, u32 count, const void *buffer);

/* Internal: register a device (called by drivers) */
void blkdev_register(u32 id, u64 total_sectors, const blkdev_ops_t *ops, void *private);

#endif
