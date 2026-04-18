#include "libsha1.h"

#include <string.h>

static uint32_t sha1_rotl32(uint32_t value, unsigned int bits) {
    return (value << bits) | (value >> (32U - bits));
}

static uint32_t sha1_load_be32(const unsigned char *input) {
    return ((uint32_t) input[0] << 24) |
           ((uint32_t) input[1] << 16) |
           ((uint32_t) input[2] << 8) |
           (uint32_t) input[3];
}

static void sha1_store_be32(unsigned char *output, uint32_t value) {
    output[0] = (unsigned char) (value >> 24);
    output[1] = (unsigned char) (value >> 16);
    output[2] = (unsigned char) (value >> 8);
    output[3] = (unsigned char) value;
}

static void sha1_store_be64(unsigned char *output, uint64_t value) {
    output[0] = (unsigned char) (value >> 56);
    output[1] = (unsigned char) (value >> 48);
    output[2] = (unsigned char) (value >> 40);
    output[3] = (unsigned char) (value >> 32);
    output[4] = (unsigned char) (value >> 24);
    output[5] = (unsigned char) (value >> 16);
    output[6] = (unsigned char) (value >> 8);
    output[7] = (unsigned char) value;
}

static void sha1_transform(sha1_ctx *ctx, const unsigned char block[64]) {
    uint32_t schedule[80];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    unsigned int i;

    for (i = 0; i < 16; ++i) {
        schedule[i] = sha1_load_be32(block + (i * 4U));
    }

    for (i = 16; i < 80; ++i) {
        schedule[i] = sha1_rotl32(schedule[i - 3] ^ schedule[i - 8] ^
                                  schedule[i - 14] ^ schedule[i - 16], 1);
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];

    for (i = 0; i < 80; ++i) {
        uint32_t function;
        uint32_t constant;
        uint32_t temp;

        if (i < 20) {
            function = (b & c) | ((~b) & d);
            constant = 0x5A827999U;
        }
        else if (i < 40) {
            function = b ^ c ^ d;
            constant = 0x6ED9EBA1U;
        }
        else if (i < 60) {
            function = (b & c) | (b & d) | (c & d);
            constant = 0x8F1BBCDCU;
        }
        else {
            function = b ^ c ^ d;
            constant = 0xCA62C1D6U;
        }

        temp = sha1_rotl32(a, 5) + function + e + constant + schedule[i];
        e = d;
        d = c;
        c = sha1_rotl32(b, 30);
        b = a;
        a = temp;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
}

void sha1_begin(sha1_ctx *ctx) {
    ctx->state[0] = 0x67452301U;
    ctx->state[1] = 0xEFCDAB89U;
    ctx->state[2] = 0x98BADCFEU;
    ctx->state[3] = 0x10325476U;
    ctx->state[4] = 0xC3D2E1F0U;
    ctx->total_len = 0;
    ctx->buffer_len = 0;
}

void sha1_hash(const void *data, size_t len, sha1_ctx *ctx) {
    const unsigned char *bytes = (const unsigned char *) data;

    ctx->total_len += (uint64_t) len;

    if (ctx->buffer_len != 0) {
        size_t needed = 64U - ctx->buffer_len;
        if (needed > len) {
            needed = len;
        }

        memcpy(ctx->buffer + ctx->buffer_len, bytes, needed);
        ctx->buffer_len += needed;
        bytes += needed;
        len -= needed;

        if (ctx->buffer_len == 64U) {
            sha1_transform(ctx, ctx->buffer);
            ctx->buffer_len = 0;
        }
    }

    while (len >= 64U) {
        sha1_transform(ctx, bytes);
        bytes += 64U;
        len -= 64U;
    }

    if (len != 0) {
        memcpy(ctx->buffer, bytes, len);
        ctx->buffer_len = len;
    }
}

void sha1_end(unsigned char digest[20], sha1_ctx *ctx) {
    unsigned char tail[128];
    size_t tail_len = 0;
    uint64_t bit_len = ctx->total_len * 8U;
    unsigned int i;

    memset(tail, 0, sizeof(tail));
    if (ctx->buffer_len != 0) {
        memcpy(tail, ctx->buffer, ctx->buffer_len);
        tail_len = ctx->buffer_len;
    }

    tail[tail_len++] = 0x80U;
    if (tail_len > 56U) {
        memset(tail + tail_len, 0, 64U - tail_len);
        sha1_transform(ctx, tail);
        tail_len = 0;
    }

    memset(tail + tail_len, 0, 56U - tail_len);
    sha1_store_be64(tail + 56U, bit_len);
    sha1_transform(ctx, tail);

    for (i = 0; i < 5; ++i) {
        sha1_store_be32(digest + (i * 4U), ctx->state[i]);
    }

    memset(ctx, 0, sizeof(*ctx));
}
