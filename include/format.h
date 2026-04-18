#ifndef OHOS_FORMAT_H
#define OHOS_FORMAT_H

#include <stdarg.h>

#include "types.h"

int vsnprintf(char *buffer, size_t size, const char *fmt, va_list args);
int snprintf(char *buffer, size_t size, const char *fmt, ...);

#endif
