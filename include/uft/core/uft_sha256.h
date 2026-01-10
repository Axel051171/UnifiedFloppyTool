/**
 * @file uft_sha256.h
 * @brief SHA-256 Hash Implementation
 * @version 1.0.0
 * 
 * Self-contained SHA-256 for forensic verification.
 * Used by recovery snapshot system.
 */

#ifndef UFT_SHA256_H
#define UFT_SHA256_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SHA-256 context structure
 */
typedef struct {
    uint32_t s[8];       /**< State */
    uint64_t bits;       /**< Total bits processed */
    uint8_t  buf[64];    /**< Buffer */
    size_t   used;       /**< Bytes in buffer */
} uft_sha256_ctx_t;

/**
 * @brief Initialize SHA-256 context
 * @param ctx Context to initialize
 */
void uft_sha256_init(uft_sha256_ctx_t *ctx);

/**
 * @brief Update hash with data
 * @param ctx Context
 * @param data Data to hash
 * @param len Data length
 */
void uft_sha256_update(uft_sha256_ctx_t *ctx, const void *data, size_t len);

/**
 * @brief Finalize and get hash
 * @param ctx Context
 * @param out Output buffer (32 bytes)
 */
void uft_sha256_final(uft_sha256_ctx_t *ctx, uint8_t out[32]);

/**
 * @brief One-shot SHA-256 hash
 * @param data Data to hash
 * @param len Data length
 * @param out Output buffer (32 bytes)
 */
void uft_sha256(const void *data, size_t len, uint8_t out[32]);

/**
 * @brief Format SHA-256 hash as hex string
 * @param hash 32-byte hash
 * @param out Output buffer (65 bytes minimum)
 */
void uft_sha256_to_hex(const uint8_t hash[32], char out[65]);

/**
 * @brief Compare two SHA-256 hashes
 * @return 0 if equal, non-zero otherwise
 */
int uft_sha256_compare(const uint8_t a[32], const uint8_t b[32]);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SHA256_H */
