#ifndef OHOS_PIT_H
#define OHOS_PIT_H

#include "types.h"

void pit_init(u32 frequency_hz);
u32 pit_ticks(void);
u32 pit_frequency(void);
void pit_sleep(u32 ticks);

#endif
