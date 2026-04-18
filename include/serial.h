#ifndef OHOS_SERIAL_H
#define OHOS_SERIAL_H

#include "types.h"

void serial_init(void);
bool serial_is_ready(void);
bool serial_can_read(void);
char serial_read_char(void);
void serial_write_char(char ch);
void serial_write(const char *text);
void serial_write_buffer(const char *text, size_t length);

#endif
