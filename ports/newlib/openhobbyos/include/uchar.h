#ifndef OPENHOBBYOS_UCHAR_H
#define OPENHOBBYOS_UCHAR_H

#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

typedef __CHAR16_TYPE__ char16_t;
typedef __CHAR32_TYPE__ char32_t;

size_t c16rtomb(char *restrict s, char16_t c16, mbstate_t *restrict ps);
size_t c32rtomb(char *restrict s, char32_t c32, mbstate_t *restrict ps);

#endif
