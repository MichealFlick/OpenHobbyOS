#ifndef OHOS_ELF_H
#define OHOS_ELF_H

#include "initrd.h"
#include "types.h"

typedef struct {
    u32 entry;
    u32 image_end;
} elf_image_t;

bool elf_load_image(const initrd_file_t *file, u32 user_base, u32 user_limit, elf_image_t *image);

#endif
