/**
 * @file uft_crc_extended.h
 * @brief Extended CRC Polynomial Support
 * 
 * Multiple CRC algorithms with configurable polynomials.
 */

#ifndef UFT_CRC_EXTENDED_H
#define UFT_CRC_EXTENDED_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CRC algorithm definition
 */
typedef struct {
    const char *name;       /**< Algorithm name */
    int width;              /**< CRC width in bits */
    uint64_t poly;          /**< Polynomial */
    uint64_t init;          /**< Initial value */
    uint64_t xor_out;       /**< XOR output mask */
    bool ref_in;            /**< Reflect input */
    bool ref_out;           /**< Reflect output */
} uft_crc_def_t;

/**
 * @brief CRC context for incremental calculation
 */
typedef struct {
    uft_crc_def_t def;         /**< Algorithm definition (copy) */
    uint64_t crc;              /**< Current CRC value */
    uint64_t *table;           /**< Lookup table */
    bool table_ready;          /**< Table initialized */
} uft_crc_ctx_t;

/* Table generation */
void uft_crc_generate_table(const uft_crc_def_t *def, uint64_t *table);

/* Context-based API */
int uft_crc_ctx_init(uft_crc_ctx_t *ctx, const uft_crc_def_t *def);
void uft_crc_ctx_free(uft_crc_ctx_t *ctx);
void uft_crc_ctx_update(uft_crc_ctx_t *ctx, const uint8_t *data, size_t length);
void uft_crc_ctx_reset(uft_crc_ctx_t *ctx);
uint64_t uft_crc_ctx_final(uft_crc_ctx_t *ctx);

/* One-shot calculation */
uint64_t uft_crc_calc(const uft_crc_def_t *def, const uint8_t *data, size_t length);
uint64_t uft_crc_calc_bits(const uft_crc_def_t *def, const uint8_t *data, size_t bit_length);

/* Algorithm lookup */
const uft_crc_def_t *uft_crc_find_by_name(const char *name);
int uft_crc_list_algorithms(const uft_crc_def_t **list, size_t *count);

/* Utility functions */
uint64_t uft_crc_reflect(uint64_t value, int width);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CRC_EXTENDED_H */
