#ifndef OHOS_PIC_H
#define OHOS_PIC_H

#include "types.h"

void pic_remap(void);
void pic_send_eoi(u8 irq);
void pic_set_mask(u8 irq);
void pic_clear_mask(u8 irq);

#endif
