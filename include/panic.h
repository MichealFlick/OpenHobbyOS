#ifndef OHOS_PANIC_H
#define OHOS_PANIC_H

#include <stdarg.h>

#include "types.h"

NORETURN void panic(const char *fmt, ...);
NORETURN void panic_v(const char *fmt, va_list args);

#endif
