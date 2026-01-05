/**
 * @file uft_fm_encoder.h
 * @brief FM (Frequency Modulation) Encoding API
 * 
 * Part of UnifiedFloppyTool v3.3.7
 * 
 * FM encoding functions for writing to Single Density floppy disks.
 * Supports IBM 3740, TRS-80, and CP/M formats.
 */

#ifndef UFT_FM_ENCODER_H
#define UFT_FM_ENCODER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uft/uft_sector.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// CONSTANTS
// ============================================================================

/** FM Address Marks */
#define UFT_FM_MARK_IAM     0xFC    /**< Index Address Mark */
#define UFT_FM_MARK_IDAM    0xFE    /**< ID Address Mark */
#define UFT_FM_MARK_DAM     0xFB    /**< Data Address Mark */
#define UFT_FM_MARK_DDAM    0xF8    /**< Deleted Data Address Mark */

/** Standard gap sizes (bytes, not FM-encoded) */
#define UFT_FM_GAP1_SIZE    40
#define UFT_FM_GAP2_SIZE    11
#define UFT_FM_GAP3_SIZE    27
#define UFT_FM_GAP4A_SIZE   26

// ============================================================================
// ENCODING FUNCTIONS
// ============================================================================

/**
 * @brief Encode a sector ID field in FM format
 * 
 * @param cyl Cylinder number
 * @param head Head number
 * @param sector Sector number
 * @param size_code Size code (0=128, 1=256, 2=512, 3=1024)
 * @param output Output buffer
 * @param output_len Output buffer length
 * @return Number of bytes written, or 0 on error
 */
size_t uft_fm_encode_sector_id(
    uint8_t cyl,
    uint8_t head,
    uint8_t sector,
    uint8_t size_code,
    uint8_t* output,
    size_t output_len
);

/**
 * @brief Encode sector data field in FM format
 * 
 * @param data Sector data
 * @param data_len Data length (128, 256, 512, or 1024)
 * @param deleted true for deleted data mark
 * @param output Output buffer
 * @param output_len Output buffer length
 * @return Number of bytes written, or 0 on error
 */
size_t uft_fm_encode_sector_data(
    const uint8_t* data,
    size_t data_len,
    bool deleted,
    uint8_t* output,
    size_t output_len
);

/**
 * @brief Encode a complete FM sector (ID + gap + data)
 * 
 * @param sector Sector structure with data
 * @param output Output buffer
 * @param output_len Output buffer length
 * @return Number of bytes written, or 0 on error
 */
size_t uft_fm_encode_sector(
    const uft_sector_t* sector,
    uint8_t* output,
    size_t output_len
);

/**
 * @brief Encode a complete FM track
 * 
 * @param sectors Array of sectors
 * @param sector_count Number of sectors
 * @param output Output buffer
 * @param output_len Output buffer length
 * @return Number of bytes written, or 0 on error
 */
size_t uft_fm_encode_track(
    const uft_sector_t* sectors,
    size_t sector_count,
    uint8_t* output,
    size_t output_len
);

/**
 * @brief Get FM encoded track length for given parameters
 * 
 * @param sector_count Number of sectors
 * @param sector_size Sector data size in bytes
 * @return Required buffer size in bytes (FM encoded, so 2x raw size)
 */
size_t uft_fm_track_size(size_t sector_count, size_t sector_size);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Encode a single byte to FM
 * 
 * @param byte Data byte to encode
 * @return 16-bit FM encoded value (clock bits interleaved)
 */
static inline uint16_t uft_fm_encode_byte_raw(uint8_t byte) {
    uint16_t result = 0;
    for (int i = 7; i >= 0; i--) {
        uint8_t data_bit = (byte >> i) & 1;
        // FM: clock is always 1
        result = (result << 2) | (1 << 1) | data_bit;
    }
    return result;
}

/**
 * @brief Decode FM byte to raw data
 * 
 * @param fm FM encoded 16-bit value
 * @return Decoded data byte
 */
static inline uint8_t uft_fm_decode_byte_raw(uint16_t fm) {
    uint8_t result = 0;
    for (int i = 0; i < 8; i++) {
        // Extract data bit (LSB of each 2-bit pair)
        if (fm & (1 << (i * 2))) {
            result |= (1 << i);
        }
    }
    return result;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_FM_ENCODER_H */
