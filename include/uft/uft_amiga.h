#ifndef UFT_AMIGA_H
#define UFT_AMIGA_H

/**
 * @file uft_amiga.h
 * @brief Amiga-specific helpers (boot block identification, checksum).
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parsed boot block information (first 1024 bytes).
 */
typedef struct uft_amiga_bootblock_info {
    int has_dos_magic;          /**< 1 if starts with 'DOS' */
    int dos_type;               /**< 0=OFS, 1=FFS, 2=FFS+Intl, 3=FFS+DirCache, -1 unknown */
    uint32_t stored_checksum;   /**< big-endian long at offset 4 */
    uint32_t computed_checksum; /**< computed checksum (as stored), for comparison */
    int checksum_ok;            /**< 1 if checksum matches */
} uft_amiga_bootblock_info_t;

/**
 * @brief Result of a generic Amiga filesystem block checksum test.
 *
 * For many OFS/FFS blocks, the checksum long is chosen such that the 32-bit sum
 * of all 128 big-endian longs in the 512-byte block equals 0.
 */
typedef struct uft_amiga_block_checksum_info {
    uint32_t sum;      /**< 32-bit wraparound sum of all longs (including checksum). */
    int checksum_ok;   /**< 1 if sum == 0, else 0. */
} uft_amiga_block_checksum_info_t;

/**
 * @brief Compute Amiga boot block checksum (1024 bytes / 256 longs).
 *
 * Algorithm: set checksum field (long at offset 4) to 0, sum all 256 big-endian
 * longs with 32-bit wraparound, then return bitwise NOT of that sum.
 */
uint32_t uft_amiga_bootblock_checksum(const uint8_t *boot1024);

/**
 * @brief Parse boot block and validate checksum.
 * @return 1 on success (enough bytes), 0 otherwise.
 */
int uft_amiga_bootblock_parse(const uint8_t *buf, size_t size, uft_amiga_bootblock_info_t *out);

/**
 * @brief Check a 512-byte Amiga filesystem block checksum.
 * @return 1 if enough bytes are provided and out is filled, 0 otherwise.
 */
int uft_amiga_block_checksum_check(const uint8_t *block512, size_t size, uft_amiga_block_checksum_info_t *out);

#ifdef __cplusplus
}
#endif

#endif /* UFT_AMIGA_H */
