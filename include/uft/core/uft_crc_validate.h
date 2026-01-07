/**
 * @file uft_crc_validate.h
 * @brief CRC Validation for Sector Reads
 * 
 * Provides CRC checking functions for sector data integrity.
 * 
 * @version 1.0.0
 * @date 2026-01-07
 */

#ifndef UFT_CRC_VALIDATE_H
#define UFT_CRC_VALIDATE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * CRC Result Structure
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint16_t    expected;       /**< Expected CRC from header */
    uint16_t    calculated;     /**< Calculated CRC from data */
    bool        valid;          /**< true if CRCs match */
    int         error_bit;      /**< Estimated error bit position (-1 if unknown) */
} uft_crc_result_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * CRC-16-CCITT (IBM MFM/FM)
 * Polynomial: 0x1021, Init: 0xFFFF
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Calculate CRC-16-CCITT
 * @param data Data buffer
 * @param len Data length
 * @return CRC-16 value
 */
static inline uint16_t uft_crc16_ccitt(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief Validate sector CRC
 * @param data Sector data (including CRC bytes at end)
 * @param data_len Data length (excluding CRC)
 * @param crc_bytes Pointer to 2-byte CRC (big-endian)
 * @param result [out] Validation result
 * @return true if CRC valid
 */
static inline bool uft_validate_sector_crc(const uint8_t *data, size_t data_len,
                                            const uint8_t *crc_bytes,
                                            uft_crc_result_t *result) {
    if (!data || !crc_bytes || !result) {
        return false;
    }
    
    result->expected = ((uint16_t)crc_bytes[0] << 8) | crc_bytes[1];
    result->calculated = uft_crc16_ccitt(data, data_len);
    result->valid = (result->expected == result->calculated);
    result->error_bit = -1;  /* Not implemented */
    
    return result->valid;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * CRC-16 for Commodore GCR (different polynomial)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Simple checksum for Commodore formats (XOR-based)
 * @param data Data buffer
 * @param len Data length
 * @return Checksum byte
 */
static inline uint8_t uft_cbm_checksum(const uint8_t *data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum ^= data[i];
    }
    return sum;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Sector Read with CRC Validation
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_CRC_OK              = 0,
    UFT_CRC_MISMATCH        = -1,
    UFT_CRC_MISSING         = -2,
    UFT_CRC_INVALID_PARAM   = -3,
} uft_crc_status_t;

/**
 * @brief Validation statistics
 */
typedef struct {
    uint32_t    sectors_checked;
    uint32_t    sectors_valid;
    uint32_t    sectors_invalid;
    uint32_t    sectors_missing_crc;
} uft_crc_stats_t;

/**
 * @brief Initialize CRC statistics
 */
static inline void uft_crc_stats_init(uft_crc_stats_t *stats) {
    if (stats) {
        stats->sectors_checked = 0;
        stats->sectors_valid = 0;
        stats->sectors_invalid = 0;
        stats->sectors_missing_crc = 0;
    }
}

/**
 * @brief Update CRC statistics
 */
static inline void uft_crc_stats_update(uft_crc_stats_t *stats, uft_crc_status_t status) {
    if (!stats) return;
    stats->sectors_checked++;
    switch (status) {
        case UFT_CRC_OK: stats->sectors_valid++; break;
        case UFT_CRC_MISMATCH: stats->sectors_invalid++; break;
        case UFT_CRC_MISSING: stats->sectors_missing_crc++; break;
        default: break;
    }
}

/**
 * @brief Get CRC validation percentage
 */
static inline double uft_crc_stats_validity_pct(const uft_crc_stats_t *stats) {
    if (!stats || stats->sectors_checked == 0) return 0.0;
    return 100.0 * (double)stats->sectors_valid / (double)stats->sectors_checked;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_CRC_VALIDATE_H */
