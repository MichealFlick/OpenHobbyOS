#ifndef OHOS_MBR_H
#define OHOS_MBR_H

#include "types.h"

#define MBR_PARTITION_COUNT 4
#define MBR_BOOT_SIGNATURE 0xAA55

/* Common partition type codes */
#define MBR_TYPE_EMPTY 0x00
#define MBR_TYPE_EXT2 0x83
#define MBR_TYPE_LINUX_NATIVE 0x83
#define MBR_TYPE_EXTENDED 0x05

typedef struct {
    u8 status;
    u8 chs_start[3];
    u8 type;
    u8 chs_end[3];
    u32 lba_start;
    u32 sector_count;
} PACKED mbr_partition_t;

typedef struct {
    u8 boot_code[446];
    mbr_partition_t partitions[4];
    u16 signature;
} PACKED mbr_t;

typedef struct {
    bool present;
    u8 type;
    u32 lba_start;
    u32 sector_count;
} mbr_partition_info_t;

bool mbr_read(u32 blkdev_id, mbr_partition_info_t *partitions);
bool mbr_is_valid(u32 blkdev_id);

#endif
