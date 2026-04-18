#ifndef OHOS_POWER_H
#define OHOS_POWER_H

#include "types.h"

typedef struct {
    bool acpi_ready;
    bool can_shutdown;
    bool can_suspend;
    bool can_reboot;
    bool suspend_uses_fallback;
    bool used_emulator_fallbacks;
} power_info_t;

void power_init(void);
power_info_t power_info(void);
bool power_shutdown(void);
bool power_suspend(void);
NORETURN void power_reboot(void);

#endif
