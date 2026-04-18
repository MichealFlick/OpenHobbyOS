#include "elf.h"

#include "string.h"

typedef struct PACKED {
    u8 ident[16];
    u16 type;
    u16 machine;
    u32 version;
    u32 entry;
    u32 phoff;
    u32 shoff;
    u32 flags;
    u16 ehsize;
    u16 phentsize;
    u16 phnum;
    u16 shentsize;
    u16 shnum;
    u16 shstrndx;
} elf_header_t;

typedef struct PACKED {
    u32 type;
    u32 offset;
    u32 vaddr;
    u32 paddr;
    u32 filesz;
    u32 memsz;
    u32 flags;
    u32 align;
} elf_program_header_t;

static u32 align_up(u32 value, u32 alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

bool elf_load_image(const initrd_file_t *file, u32 user_base, u32 user_limit, elf_image_t *image) {
    const elf_header_t *header;
    u32 image_end = user_base;

    if (!file || file->size < sizeof(elf_header_t) || !image) {
        return false;
    }

    header = (const elf_header_t *)file->data;
    if (header->ident[0] != 0x7F || header->ident[1] != 'E' || header->ident[2] != 'L' || header->ident[3] != 'F') {
        return false;
    }
    if (header->ident[4] != 1 || header->ident[5] != 1) {
        return false;
    }
    if (header->machine != 3 || header->type != 2 || header->version != 1) {
        return false;
    }
    if (header->phoff + header->phnum * sizeof(elf_program_header_t) > file->size) {
        return false;
    }

    for (u16 i = 0; i < header->phnum; ++i) {
        const elf_program_header_t *ph =
            (const elf_program_header_t *)(file->data + header->phoff + i * sizeof(elf_program_header_t));

        if (ph->type != 1) {
            continue;
        }
        if (ph->offset + ph->filesz > file->size) {
            return false;
        }
        if (ph->vaddr < user_base || ph->vaddr + ph->memsz > user_limit || ph->vaddr + ph->memsz < ph->vaddr) {
            return false;
        }

        /* Fixed-address loading keeps the ABI surface real without pretending we already have paging. */
        memset((void *)(uintptr_t)ph->vaddr, 0, ph->memsz);
        memcpy((void *)(uintptr_t)ph->vaddr, file->data + ph->offset, ph->filesz);

        if (ph->vaddr + ph->memsz > image_end) {
            image_end = ph->vaddr + ph->memsz;
        }
    }

    if (header->entry < user_base || header->entry >= user_limit) {
        return false;
    }

    image->entry = header->entry;
    image->image_end = align_up(image_end, 4096);
    return true;
}
