#ifndef OHOS_USER_RUNTIME_H
#define OHOS_USER_RUNTIME_H

#include "abi/linux.h"

unsigned int u_strlen(const char *text);
int u_strcmp(const char *left, const char *right);
int u_strncmp(const char *left, const char *right, unsigned int count);
char *u_strcpy(char *dest, const char *src);
void *u_memset(void *dest, int value, unsigned int length);
void *u_memcpy(void *dest, const void *src, unsigned int length);
void u_puts(const char *text);
void u_putsn(const char *text, unsigned int length);
void u_put_uint(unsigned int value);
void u_put_hex(unsigned int value);
void u_put_int(int value);
void u_put_u64(unsigned long long value);
void u_print_uname(const struct linux_utsname *name);
const char *u_basename(const char *path);
void u_print_stat(const char *label, const struct linux_stat64 *stat);
unsigned int u_parse_uint(const char *text, int *ok);

#endif
