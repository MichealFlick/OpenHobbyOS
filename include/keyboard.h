#ifndef OHOS_KEYBOARD_H
#define OHOS_KEYBOARD_H

#include "types.h"

void keyboard_init(void);
size_t keyboard_readline(char *buffer, size_t size);
bool keyboard_has_input(void);
char keyboard_getchar(void);
bool keyboard_has_raw_scancode(void);
u8 keyboard_read_raw_scancode(void);

#endif
