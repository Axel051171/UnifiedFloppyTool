/**
 * @file uft_crc_v2.h
 * @brief Unified CRC Library - All CRC/Checksum algorithms in one place
 * 
 * This consolidates all CRC implementations from:
 * - uft_crc.c (original)
 * - pc_mfm.c (crc16_ccitt, crc16_with_sync)
 * - error_correction.c (fast_crc16)
 * - bootblock_crc32.c (bb_crc32)
 * - uft_ipf.c (ipf_crc32)
 * - cpc_mfm.c (crc16_ccitt_update)
 * - mfm_ibm_encode.c (crc16_ccitt_update)
 * 
 * Benefits:
 * - Single source of truth
 * - Table-based fast implementations
 * - Thread-safe (no mutable globals)
 * - Comprehensive test coverage
 * 
 * @version 2.0
 * @date 2026-01-07
 */

#ifndef UFT_CRC_V2_H
#define UFT_CRC_V2_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * CRC-16 CCITT (X.25, HDLC, etc.)
 * Polynomial: x^16 + x^12 + x^5 + 1 (0x1021)
 *===========================================================================*/

/**
 * @brief CRC-16 CCITT lookup table (pre-computed)
 * 
 * This table is const and therefore thread-safe.
 * Polynomial: 0x1021 (standard CCITT)
 */
extern const uint16_t UFT_CRC16_TABLE[256];

/**
 * @brief Update CRC-16 with one byte (table-based, fast)
 */
static inline uint16_t uft_crc16_update_fast(uint16_t crc, uint8_t byte) {
    extern const uint16_t UFT_CRC16_TABLE[256];
    return (crc << 8) ^ UFT_CRC16_TABLE[((crc >> 8) ^ byte) & 0xFF];
}

/**
 * @brief Update CRC-16 with one byte (no table, small code)
 */
static inline uint16_t uft_crc16_update_small(uint16_t crc, uint8_t byte) {
    crc = ((crc >> 8) & 0xFF) | (crc << 8);
    crc ^= byte;
    crc ^= (crc & 0xFF) >> 4;
    crc ^= crc << 12;
    crc ^= (crc & 0xFF) << 5;
    return crc;
}

/**
 * @brief Calculate CRC-16 CCITT over data block
 * 
 * @param data   Input data
 * @param len    Length in bytes
 * @param init   Initial CRC value (typically 0xFFFF)
 * @return       Computed CRC-16
 */
uint16_t uft_crc16_calc(const uint8_t *data, size_t len, uint16_t init);

/**
 * @brief CRC-16 with MFM sync bytes pre-loaded
 * 
 * For IBM MFM sectors, the CRC includes the sync pattern (A1 A1 A1)
 * already in the initial value.
 * 
 * @param data   Sector data (starting with address mark)
 * @param len    Length in bytes
 * @return       CRC-16 including sync pattern
 */
uint16_t uft_crc16_mfm_sync(const uint8_t *data, size_t len);

/**
 * @brief Verify CRC-16 (returns true if valid)
 */
static inline bool uft_crc16_verify(const uint8_t *data, size_t len, 
                                    uint16_t init, uint16_t expected) {
    return uft_crc16_calc(data, len, init) == expected;
}

/*===========================================================================
 * CRC-32 (ISO 3309, PNG, ZIP, etc.)
 * Polynomial: 0x04C11DB7 (reflected: 0xEDB88320)
 *===========================================================================*/

/**
 * @brief CRC-32 lookup table (pre-computed)
 */
extern const uint32_t UFT_CRC32_TABLE[256];

/**
 * @brief Update CRC-32 with one byte
 */
static inline uint32_t uft_crc32_update(uint32_t crc, uint8_t byte) {
    extern const uint32_t UFT_CRC32_TABLE[256];
    return (crc >> 8) ^ UFT_CRC32_TABLE[(crc ^ byte) & 0xFF];
}

/**
 * @brief Calculate CRC-32 over data block
 * 
 * @param data   Input data
 * @param len    Length in bytes
 * @return       Computed CRC-32 (already XORed with 0xFFFFFFFF)
 */
uint32_t uft_crc32_calc(const uint8_t *data, size_t len);

/**
 * @brief Calculate CRC-32 with custom initial value
 */
uint32_t uft_crc32_calc_init(const uint8_t *data, size_t len, uint32_t init);

/**
 * @brief Verify CRC-32
 */
static inline bool uft_crc32_verify(const uint8_t *data, size_t len, 
                                    uint32_t expected) {
    return uft_crc32_calc(data, len) == expected;
}

/*===========================================================================
 * Platform-Specific Checksums
 *===========================================================================*/

/**
 * @brief XOR checksum (C64, Apple II, etc.)
 */
static inline uint8_t uft_checksum_xor(const uint8_t *data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum ^= data[i];
    }
    return sum;
}

/**
 * @brief C64 header checksum (track ^ sector ^ id1 ^ id2)
 */
static inline uint8_t uft_c64_header_checksum(uint8_t track, uint8_t sector,
                                               uint8_t id1, uint8_t id2) {
    return track ^ sector ^ id1 ^ id2;
}

/**
 * @brief C64 data sector checksum (XOR of 256 bytes)
 */
static inline uint8_t uft_c64_data_checksum(const uint8_t *data) {
    return uft_checksum_xor(data, 256);
}

/**
 * @brief Apple II 4-4 checksum
 */
static inline uint8_t uft_apple_checksum(uint8_t volume, uint8_t track,
                                          uint8_t sector) {
    return volume ^ track ^ sector;
}

/**
 * @brief Amiga sector checksum (XOR of 32-bit words, big-endian)
 */
uint32_t uft_amiga_sector_checksum(const uint8_t *data, size_t len);

/**
 * @brief Amiga bootblock checksum
 * 
 * Special algorithm: 32-bit sum with carry propagation
 * 
 * @param bootblock  1024 bytes of boot block data
 * @return           Computed checksum
 */
uint32_t uft_amiga_bootblock_checksum(const uint8_t *bootblock);

/**
 * @brief Verify Amiga bootblock checksum
 * 
 * Checksum is stored at offset 4 in the bootblock.
 */
bool uft_amiga_bootblock_verify(const uint8_t *bootblock);

/**
 * @brief Fix Amiga bootblock checksum
 * 
 * Calculates correct checksum and writes it to offset 4.
 */
void uft_amiga_bootblock_fix(uint8_t *bootblock);

/*===========================================================================
 * Fletcher & Adler Checksums
 *===========================================================================*/

/**
 * @brief Fletcher-16 checksum
 */
uint16_t uft_fletcher16(const uint8_t *data, size_t len);

/**
 * @brief Adler-32 checksum
 */
uint32_t uft_adler32(const uint8_t *data, size_t len);

/*===========================================================================
 * CRC Error Correction
 *===========================================================================*/

/**
 * @brief Maximum correctable bit errors
 */
#define UFT_CRC_MAX_ERRORS 8

/**
 * @brief CRC correction result
 */
typedef struct {
    bool     corrected;                      /**< True if corrected */
    uint8_t  error_count;                    /**< Number of errors found */
    uint16_t error_positions[UFT_CRC_MAX_ERRORS]; /**< Bit positions */
    float    confidence;                     /**< 0.0 - 1.0 */
} uft_crc_correction_t;

/**
 * @brief Attempt to correct CRC-16 errors (single-bit only)
 * 
 * @param data       Data to correct (modified in place)
 * @param len        Data length
 * @param expected   Expected CRC value
 * @param result     Correction result (optional, can be NULL)
 * @return           true if corrected successfully
 */
bool uft_crc16_correct(uint8_t *data, size_t len, 
                       uint16_t expected, uft_crc_correction_t *result);

/*===========================================================================
 * Utility
 *===========================================================================*/

/**
 * @brief Read 16-bit big-endian value
 */
static inline uint16_t uft_read_be16(const uint8_t *p) {
    return ((uint16_t)p[0] << 8) | p[1];
}

/**
 * @brief Read 32-bit big-endian value
 */
static inline uint32_t uft_read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

/**
 * @brief Write 16-bit big-endian value
 */
static inline void uft_write_be16(uint8_t *p, uint16_t val) {
    p[0] = (uint8_t)(val >> 8);
    p[1] = (uint8_t)val;
}

/**
 * @brief Write 32-bit big-endian value
 */
static inline void uft_write_be32(uint8_t *p, uint32_t val) {
    p[0] = (uint8_t)(val >> 24);
    p[1] = (uint8_t)(val >> 16);
    p[2] = (uint8_t)(val >> 8);
    p[3] = (uint8_t)val;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_CRC_V2_H */
