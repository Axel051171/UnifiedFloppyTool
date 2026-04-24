/**
 * @file uft_sha256.c
 * @brief SHA-256 Hash Implementation
 * @version 1.0.0
 * 
 * Self-contained SHA-256 implementation (no external dependencies).
 * Based on FIPS 180-4 specification.
 */

#include "uft/core/uft_sha256.h"
#include <string.h>

/* Rotate right */
static uint32_t rotr(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

/* SHA-256 functions */
static uint32_t ch(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}

static uint32_t maj(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

static uint32_t bsig0(uint32_t x) {
    return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

static uint32_t bsig1(uint32_t x) {
    return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

static uint32_t ssig0(uint32_t x) {
    return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}

static uint32_t ssig1(uint32_t x) {
    return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

/* Round constants */
static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

/* Process a 64-byte block */
static void compress(uft_sha256_ctx_t *ctx, const uint8_t block[64]) {
    uint32_t w[64];
    
    /* Prepare message schedule */
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4 + 0] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) |
               ((uint32_t)block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; i++) {
        w[i] = ssig1(w[i - 2]) + w[i - 7] + ssig0(w[i - 15]) + w[i - 16];
    }
    
    /* Initialize working variables */
    uint32_t a = ctx->s[0], b = ctx->s[1], c = ctx->s[2], d = ctx->s[3];
    uint32_t e = ctx->s[4], f = ctx->s[5], g = ctx->s[6], h = ctx->s[7];
    
    /* 64 rounds */
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + bsig1(e) + ch(e, f, g) + K[i] + w[i];
        uint32_t t2 = bsig0(a) + maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    
    /* Update state */
    ctx->s[0] += a;
    ctx->s[1] += b;
    ctx->s[2] += c;
    ctx->s[3] += d;
    ctx->s[4] += e;
    ctx->s[5] += f;
    ctx->s[6] += g;
    ctx->s[7] += h;
}

void uft_sha256_init(uft_sha256_ctx_t *ctx) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->s[0] = 0x6a09e667;
    ctx->s[1] = 0xbb67ae85;
    ctx->s[2] = 0x3c6ef372;
    ctx->s[3] = 0xa54ff53a;
    ctx->s[4] = 0x510e527f;
    ctx->s[5] = 0x9b05688c;
    ctx->s[6] = 0x1f83d9ab;
    ctx->s[7] = 0x5be0cd19;
    ctx->bits = 0;
    ctx->used = 0;
}

void uft_sha256_update(uft_sha256_ctx_t *ctx, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    
    while (len) {
        size_t take = 64 - ctx->used;
        if (take > len) take = len;
        
        memcpy(ctx->buf + ctx->used, p, take);
        ctx->used += take;
        p += take;
        len -= take;
        
        if (ctx->used == 64) {
            compress(ctx, ctx->buf);
            ctx->bits += 512;
            ctx->used = 0;
        }
    }
}

void uft_sha256_final(uft_sha256_ctx_t *ctx, uint8_t out[32]) {
    uint64_t total_bits = ctx->bits + (uint64_t)ctx->used * 8;
    
    /* Append 1 bit */
    ctx->buf[ctx->used++] = 0x80;
    
    /* Pad to 56 bytes (448 bits) */
    if (ctx->used > 56) {
        while (ctx->used < 64) ctx->buf[ctx->used++] = 0;
        compress(ctx, ctx->buf);
        ctx->bits += 512;
        ctx->used = 0;
    }
    while (ctx->used < 56) ctx->buf[ctx->used++] = 0;
    
    /* Append length (big-endian) */
    for (int i = 0; i < 8; i++) {
        ctx->buf[63 - i] = (uint8_t)(total_bits >> (i * 8));
    }
    
    compress(ctx, ctx->buf);
    
    /* Output hash (big-endian) */
    for (int i = 0; i < 8; i++) {
        out[i * 4 + 0] = (uint8_t)(ctx->s[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(ctx->s[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(ctx->s[i] >> 8);
        out[i * 4 + 3] = (uint8_t)(ctx->s[i]);
    }
}

void uft_sha256(const void *data, size_t len, uint8_t out[32]) {
    uft_sha256_ctx_t ctx;
    uft_sha256_init(&ctx);
    uft_sha256_update(&ctx, data, len);
    uft_sha256_final(&ctx, out);
}

void uft_sha256_to_hex(const uint8_t hash[32], char out[65]) {
    static const char *hex = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        out[i * 2] = hex[(hash[i] >> 4) & 0xF];
        out[i * 2 + 1] = hex[hash[i] & 0xF];
    }
    out[64] = '\0';
}

int uft_sha256_compare(const uint8_t a[32], const uint8_t b[32]) {
    return memcmp(a, b, 32);
}
