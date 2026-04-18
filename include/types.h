#ifndef OHOS_TYPES_H
#define OHOS_TYPES_H

typedef unsigned char u8;
typedef signed char i8;
typedef unsigned short u16;
typedef signed short i16;
typedef unsigned int u32;
typedef signed int i32;
typedef unsigned long long u64;
typedef signed long long i64;
typedef unsigned int size_t;
typedef signed int ssize_t;
typedef unsigned int uintptr_t;

typedef enum {
    false = 0,
    true = 1,
} bool;

#define NULL ((void *)0)

#define PACKED __attribute__((packed))
#define NORETURN __attribute__((noreturn))
#define UNUSED __attribute__((unused))

#endif
