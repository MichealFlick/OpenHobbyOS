#ifndef OHOS_TYPES_H
#define OHOS_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef unsigned char u8;
typedef signed char i8;
typedef unsigned short u16;
typedef signed short i16;
typedef unsigned int u32;
typedef signed int i32;
typedef unsigned long long u64;
typedef signed long long i64;
typedef signed int ssize_t;

#ifndef __bool_true_false_are_defined
typedef _Bool bool;
#define true 1
#define false 0
#define __bool_true_false_are_defined 1
#endif

#define NULL ((void *)0)

#define PACKED __attribute__((packed))
#define NORETURN __attribute__((noreturn))
#define UNUSED __attribute__((unused))

#endif
