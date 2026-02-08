/**
 * @file uft_mfm_flux.h
 * @brief MFM Flux Decoding Constants and Functions
 */

#ifndef UFT_MFM_FLUX_H
#define UFT_MFM_FLUX_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * CRC-16-CCITT Constants
 *===========================================================================*/

#define UFT_CRC16_POLY      0x1021
#define UFT_CRC16_INIT      0xFFFF
#define UFT_CRC16_INIT_IBM  0xFFFF
#define UFT_CRC16_INIT_ZERO 0x0000

/*===========================================================================
 * MFM Address Mark Constants
 *===========================================================================*/

#define MFM_SYNC_WORD       0x4489
#define MFM_GAP_BYTE        0x4E

#define UFT_MFM_SYNC_A1     0xA1    /**< Sync byte with missing clock */
#define UFT_MFM_SYNC_C2     0xC2    /**< Index sync byte */
#define UFT_MFM_MARK_IAM    0xFC    /**< Index address mark */
#define UFT_MFM_MARK_IDAM   0xFE    /**< ID address mark */
#define UFT_MFM_MARK_DAM    0xFB    /**< Data address mark */
#define UFT_MFM_MARK_DDAM   0xF8    /**< Deleted data address mark */

/*===========================================================================
 * CRC-16-CCITT Functions
 *===========================================================================*/

/** CRC-16 lookup table */
extern const uint16_t uft_mfm_crc16_table[256];

/** Compute CRC-16-CCITT using lookup table */
static inline uint16_t uft_mfm_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = UFT_CRC16_INIT;
    for (size_t i = 0; i < len; i++) {
        crc = (crc << 8) ^ uft_mfm_crc16_table[(crc >> 8) ^ data[i]];
    }
    return crc;
}

/** Initialize CRC context */
static inline void uft_mfm_crc16_init(uint16_t *crc)
{
    *crc = UFT_CRC16_INIT;
}

/** Update CRC with one byte */
static inline void uft_mfm_crc16_update(uint16_t *crc, uint8_t byte)
{
    *crc = (*crc << 8) ^ uft_mfm_crc16_table[(*crc >> 8) ^ byte];
}

/** Verify CRC (returns true if valid) */
static inline bool uft_mfm_crc16_verify(uint16_t crc)
{
    return crc == 0;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_MFM_FLUX_H */
