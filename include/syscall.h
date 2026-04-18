#ifndef OHOS_SYSCALL_H
#define OHOS_SYSCALL_H

#include "idt.h"

int syscall_dispatch(registers_t *regs);

#endif
