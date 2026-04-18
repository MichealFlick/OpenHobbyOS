#ifndef OPENHOBBYOS_LIBSHA1_H
#define OPENHOBBYOS_LIBSHA1_H

#include <stddef.h>
#include <stdint.h>

typedef struct sha1_ctx {
    uint32_t state[5];
    uint64_t total_len;
    size_t buffer_len;
    unsigned char buffer[64];
} sha1_ctx;

void sha1_begin(sha1_ctx *ctx);
void sha1_hash(const void *data, size_t len, sha1_ctx *ctx);
void sha1_end(unsigned char digest[20], sha1_ctx *ctx);

#endif
