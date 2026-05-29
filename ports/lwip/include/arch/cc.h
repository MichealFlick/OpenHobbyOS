#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LWIP_ERRNO_INCLUDE <errno.h>
#define LWIP_ERRNO_STDINCLUDE 1

#define LWIP_RAND() rand()

typedef uint32_t sys_prot_t;

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define BYTE_ORDER BIG_ENDIAN
#else
#define BYTE_ORDER LITTLE_ENDIAN
#endif

#ifdef __cplusplus
}
#endif

#endif
