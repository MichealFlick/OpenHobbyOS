#ifndef OHOS_STRING_H
#define OHOS_STRING_H

#include "types.h"

void *memcpy(void *dest, const void *src, size_t count);
void *memmove(void *dest, const void *src, size_t count);
void *memset(void *dest, int value, size_t count);
int memcmp(const void *left, const void *right, size_t count);

size_t strlen(const char *value);
int strcmp(const char *left, const char *right);
int strncmp(const char *left, const char *right, size_t count);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t count);
char *strchr(const char *value, int needle);
char *strrchr(const char *value, int needle);
bool isspace(int ch);
bool isdigit(int ch);
bool isalpha(int ch);
char tolower(char ch);
char toupper(char ch);

#endif
