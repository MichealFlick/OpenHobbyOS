#ifndef OHOS_ELF_H
#define OHOS_ELF_H

#include "initrd.h"
#include "types.h"

typedef struct {
    u32 entry;
    u32 image_end;
    u32 tls_memsz;
    u32 tls_filesz;
    u32 tls_vaddr;
} elf_image_t;

struct vfs_node;

bool elf_load_image(const initrd_file_t *file, u32 user_base, u32 user_limit, elf_image_t *image);
bool elf_load_vfs_node(const struct vfs_node *node, u32 user_base, u32 user_limit, elf_image_t *image);

#endif
