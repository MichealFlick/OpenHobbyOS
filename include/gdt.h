#ifndef OHOS_GDT_H
#define OHOS_GDT_H

#include "types.h"

#define KERNEL_CS 0x08
#define KERNEL_DS 0x10
#define USER_CS   0x1B
#define USER_DS   0x23

void gdt_init(uintptr_t kernel_stack_top);
void gdt_set_kernel_stack(uintptr_t kernel_stack_top);

#endif
