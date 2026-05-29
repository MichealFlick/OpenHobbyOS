#include "elf.h"

#include "memory.h"
#include "string.h"
#include "vfs.h"

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
    u32 load_min = 0xFFFFFFFF;
    u32 load_max = 0;

    (void)user_base;
    (void)user_limit;

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
        if (ph->vaddr + ph->memsz < ph->vaddr) {
            return false;
        }

        /* Load at ELF-specified address (must be above 1MB to avoid kernel) */
        if (ph->vaddr < 0x100000) {
            return false;
        }

        /* Fixed-address loading keeps the ABI surface real without pretending we already have paging. */
        memset((void *)(uintptr_t)ph->vaddr, 0, ph->memsz);
        memcpy((void *)(uintptr_t)ph->vaddr, file->data + ph->offset, ph->filesz);

        if (ph->vaddr < load_min) {
            load_min = ph->vaddr;
        }
        if (ph->vaddr + ph->memsz > load_max) {
            load_max = ph->vaddr + ph->memsz;
        }
    }

    if (header->entry < load_min || header->entry >= load_max) {
        return false;
    }

    image->entry = header->entry;
    image->image_end = align_up(load_max, 4096);
    image->tls_memsz = 0;
    image->tls_filesz = 0;
    image->tls_vaddr = 0;
    return true;
}

bool elf_load_vfs_node(const vfs_node_t *node, u32 user_base, u32 user_limit, elf_image_t *image) {
    const initrd_file_t *backing;

    if (!node || !image) {
        return false;
    }

    backing = vfs_backing_file(node);
    if (backing) {
        return elf_load_image(backing, user_base, user_limit, image);
    }

    if (vfs_is_dir(node)) {
        return false;
    }

    {
        u32 sz = vfs_file_size(node);
        u8 *buf;
        ssize_t n;

        if (sz == 0 || sz > user_limit - user_base) {
            return false;
        }

        buf = kmalloc(sz);
        if (!buf) {
            return false;
        }

        n = vfs_read(node, 0, buf, sz);
        if (n != (ssize_t)sz) {
            kfree(buf);
            return false;
        }

        {
            initrd_file_t tmp;
            memset(&tmp, 0, sizeof(tmp));
            tmp.data = buf;
            tmp.size = sz;

            if (!elf_load_image(&tmp, user_base, user_limit, image)) {
                kfree(buf);
                return false;
            }
        }

        kfree(buf);
        return true;
    }
}
