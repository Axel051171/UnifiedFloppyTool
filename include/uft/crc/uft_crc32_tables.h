/**
 * @file uft_crc32_tables.h
 * @brief Precomputed CRC32 Lookup Tables
 */

#ifndef UFT_CRC32_TABLES_H
#define UFT_CRC32_TABLES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CRC32 polynomial types
 */
typedef enum {
    UFT_CRC32_IEEE = 0,      /**< IEEE 802.3 (Ethernet, ZIP, PNG) */
    UFT_CRC32_CASTAGNOLI,    /**< Castagnoli (iSCSI, Btrfs, ext4) */
    UFT_CRC32_KOOPMAN,       /**< Koopman */
    UFT_CRC32_Q,             /**< CRC-32Q (aviation) */
    UFT_CRC32_XFER,          /**< CRC-32/XFER */
    UFT_CRC32_POSIX,         /**< POSIX cksum */
    UFT_CRC32_TYPE_COUNT
} uft_crc32_type_t;

/**
 * @brief CRC32 calculation context
 */
typedef struct {
    uint32_t crc;            /**< Current CRC value */
    uint32_t init;           /**< Initial value */
    uint32_t xor_out;        /**< XOR output mask */
    uft_crc32_type_t type;   /**< Polynomial type */
    const uint32_t *table;   /**< Lookup table pointer */
} uft_crc32_ctx_t;

/* Context-based API */
void uft_crc32_ctx_init(uft_crc32_ctx_t *ctx, uft_crc32_type_t type);
void uft_crc32_ctx_update(uft_crc32_ctx_t *ctx, const uint8_t *data, size_t len);
uint32_t uft_crc32_ctx_final(uft_crc32_ctx_t *ctx);
void uft_crc32_ctx_reset(uft_crc32_ctx_t *ctx);

/* One-shot functions */
uint32_t uft_crc32_calc(const uint8_t *data, size_t len);
uint32_t uft_crc32c_calc(const uint8_t *data, size_t len);

/* Table generation and access */
void uft_crc32_generate_table(uint32_t poly, uint32_t *table);
const uint32_t *uft_crc32_get_table(uft_crc32_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CRC32_TABLES_H */
