/**
 * @file uft_track_generator.h
 * @brief Track Generation and Encoding
 * 
 * License: GPL-2.0+
 * 
 * Provides low-level track generation for various encoding schemes:
 * - FM (Frequency Modulation)
 * - MFM (Modified FM)
 * - GCR (Group Coded Recording) - C64, Apple, Victor
 * 
 * Supports generation of:
 * - Gap bytes
 * - Sync patterns
 * - Address marks
 * - Sector data with CRC
 */

#ifndef UFT_TRACK_GENERATOR_H
#define UFT_TRACK_GENERATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Maximum track size in bytes (raw encoded) */
#define UFT_TRKGEN_MAX_TRACK_SIZE   32768

/** Default bit rate for DD (250 kbps) */
#define UFT_TRKGEN_BITRATE_DD       250000

/** Default bit rate for HD (500 kbps) */
#define UFT_TRKGEN_BITRATE_HD       500000

/** Default bit rate for ED (1 Mbps) */
#define UFT_TRKGEN_BITRATE_ED       1000000

/** Standard RPM values */
#define UFT_TRKGEN_RPM_300          300
#define UFT_TRKGEN_RPM_360          360

/*===========================================================================
 * Encoding Types
 *===========================================================================*/

/**
 * @brief Encoding type
 */
typedef enum {
    UFT_TRKGEN_ENC_FM,          /**< FM encoding (single density) */
    UFT_TRKGEN_ENC_MFM,         /**< MFM encoding (double/high density) */
    UFT_TRKGEN_ENC_GCR_C64,     /**< C64 GCR encoding */
    UFT_TRKGEN_ENC_GCR_APPLE,   /**< Apple II 6-and-2 GCR */
    UFT_TRKGEN_ENC_GCR_MAC,     /**< Apple Mac GCR */
    UFT_TRKGEN_ENC_GCR_VICTOR,  /**< Victor 9000 GCR */
    UFT_TRKGEN_ENC_RAW          /**< Raw bitstream (no encoding) */
} uft_trkgen_encoding_t;

/**
 * @brief Address mark type
 */
typedef enum {
    UFT_TRKGEN_AM_INDEX,        /**< Index Address Mark (IAM) */
    UFT_TRKGEN_AM_ID,           /**< ID Address Mark (IDAM) */
    UFT_TRKGEN_AM_DATA,         /**< Data Address Mark (DAM) */
    UFT_TRKGEN_AM_DELETED       /**< Deleted Data Address Mark (DDAM) */
} uft_trkgen_am_type_t;

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/**
 * @brief Track generator state
 */
typedef struct {
    uint8_t *buffer;            /**< Output bitstream buffer */
    size_t buffer_size;         /**< Buffer size in bytes */
    size_t bit_index;           /**< Current bit position */
    
    uft_trkgen_encoding_t encoding;  /**< Current encoding */
    uint32_t bitrate;           /**< Bit rate in bits/sec */
    uint16_t rpm;               /**< Drive RPM */
    
    uint8_t last_bit;           /**< Last written bit (for MFM clock) */
    uint16_t crc;               /**< Running CRC value */
    bool crc_active;            /**< CRC calculation active */
    
    /* Track metadata */
    uint8_t track_num;          /**< Track number */
    uint8_t head;               /**< Head number */
    uint8_t sector_count;       /**< Sectors written */
} uft_trkgen_t;

/**
 * @brief Sector parameters
 */
typedef struct {
    uint8_t cylinder;           /**< Cylinder number for IDAM */
    uint8_t head;               /**< Head number for IDAM */
    uint8_t sector;             /**< Sector number for IDAM */
    uint8_t size_code;          /**< Size code (0=128, 1=256, 2=512, ...) */
    const uint8_t *data;        /**< Sector data */
    size_t data_size;           /**< Actual data size */
    bool deleted;               /**< Use deleted data mark */
    uint8_t gap3_size;          /**< Gap3 size after sector */
    uint8_t filler;             /**< Gap filler byte (usually 0x4E) */
} uft_trkgen_sector_t;

/**
 * @brief Track format specification
 */
typedef struct {
    uft_trkgen_encoding_t encoding;
    uint32_t bitrate;
    uint16_t rpm;
    
    uint8_t gap1_size;          /**< Post-index gap */
    uint8_t gap2_size;          /**< Post-ID gap */
    uint8_t gap3_size;          /**< Post-data gap */
    uint8_t gap4a_size;         /**< Pre-index gap */
    
    uint8_t sync_size;          /**< Sync byte count */
    uint8_t gap_filler;         /**< Gap filler byte */
    
    uint8_t idam_byte;          /**< IDAM marker (0xFE for MFM) */
    uint8_t dam_byte;           /**< DAM marker (0xFB for MFM) */
    uint8_t ddam_byte;          /**< DDAM marker (0xF8 for MFM) */
} uft_trkgen_format_t;

/*===========================================================================
 * Standard Format Presets
 *===========================================================================*/

/** IBM PC DD format (360K/720K) */
static const uft_trkgen_format_t UFT_TRKGEN_FORMAT_IBM_DD = {
    .encoding = UFT_TRKGEN_ENC_MFM,
    .bitrate = UFT_TRKGEN_BITRATE_DD,
    .rpm = UFT_TRKGEN_RPM_300,
    .gap1_size = 50,
    .gap2_size = 22,
    .gap3_size = 80,
    .gap4a_size = 80,
    .sync_size = 12,
    .gap_filler = 0x4E,
    .idam_byte = 0xFE,
    .dam_byte = 0xFB,
    .ddam_byte = 0xF8
};

/** IBM PC HD format (1.2M/1.44M) */
static const uft_trkgen_format_t UFT_TRKGEN_FORMAT_IBM_HD = {
    .encoding = UFT_TRKGEN_ENC_MFM,
    .bitrate = UFT_TRKGEN_BITRATE_HD,
    .rpm = UFT_TRKGEN_RPM_300,
    .gap1_size = 80,
    .gap2_size = 22,
    .gap3_size = 108,
    .gap4a_size = 80,
    .sync_size = 12,
    .gap_filler = 0x4E,
    .idam_byte = 0xFE,
    .dam_byte = 0xFB,
    .ddam_byte = 0xF8
};

/** Amiga DD format */
static const uft_trkgen_format_t UFT_TRKGEN_FORMAT_AMIGA = {
    .encoding = UFT_TRKGEN_ENC_MFM,
    .bitrate = UFT_TRKGEN_BITRATE_DD,
    .rpm = UFT_TRKGEN_RPM_300,
    .gap1_size = 0,             /* Amiga uses sync-based format */
    .gap2_size = 0,
    .gap3_size = 0,
    .gap4a_size = 0,
    .sync_size = 2,
    .gap_filler = 0xAA,
    .idam_byte = 0x00,          /* Amiga uses 0x4489 sync */
    .dam_byte = 0x00,
    .ddam_byte = 0x00
};

/*===========================================================================
 * Initialization
 *===========================================================================*/

/**
 * @brief Initialize track generator
 * @param tg Track generator state
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @param format Format specification
 * @return 0 on success
 */
int uft_trkgen_init(uft_trkgen_t *tg, uint8_t *buffer, size_t buffer_size,
                    const uft_trkgen_format_t *format);

/**
 * @brief Reset generator for new track
 * @param tg Track generator state
 * @param track Track number
 * @param head Head number
 */
void uft_trkgen_reset(uft_trkgen_t *tg, uint8_t track, uint8_t head);

/*===========================================================================
 * Low-Level Bit Operations
 *===========================================================================*/

/**
 * @brief Write raw bit
 * @param tg Track generator
 * @param bit Bit value (0 or 1)
 */
void uft_trkgen_write_bit(uft_trkgen_t *tg, uint8_t bit);

/**
 * @brief Write raw bits
 * @param tg Track generator
 * @param bits Bit pattern (MSB first)
 * @param count Number of bits
 */
void uft_trkgen_write_bits(uft_trkgen_t *tg, uint32_t bits, uint8_t count);

/**
 * @brief Write FM-encoded byte
 * @param tg Track generator
 * @param byte Data byte
 * @param clock_bits Clock bit pattern (0xFF for standard)
 */
void uft_trkgen_write_fm_byte(uft_trkgen_t *tg, uint8_t byte, uint8_t clock_bits);

/**
 * @brief Write MFM-encoded byte
 * @param tg Track generator
 * @param byte Data byte
 */
void uft_trkgen_write_mfm_byte(uft_trkgen_t *tg, uint8_t byte);

/**
 * @brief Write MFM-encoded byte with clock pattern
 * @param tg Track generator
 * @param byte Data byte
 * @param clock MFM clock bits (for sync marks)
 */
void uft_trkgen_write_mfm_byte_clock(uft_trkgen_t *tg, uint8_t byte, uint16_t clock);

/*===========================================================================
 * High-Level Operations
 *===========================================================================*/

/**
 * @brief Write gap bytes
 * @param tg Track generator
 * @param count Number of gap bytes
 * @param filler Filler byte value
 */
void uft_trkgen_write_gap(uft_trkgen_t *tg, size_t count, uint8_t filler);

/**
 * @brief Write sync bytes (MFM 0x00 with clock)
 * @param tg Track generator
 * @param count Number of sync bytes
 */
void uft_trkgen_write_sync(uft_trkgen_t *tg, size_t count);

/**
 * @brief Write MFM A1 sync mark with missing clock
 * @param tg Track generator
 * @param count Number of A1 bytes (usually 3)
 */
void uft_trkgen_write_a1_sync(uft_trkgen_t *tg, size_t count);

/**
 * @brief Write address mark
 * @param tg Track generator
 * @param type Address mark type
 */
void uft_trkgen_write_address_mark(uft_trkgen_t *tg, uft_trkgen_am_type_t type);

/**
 * @brief Write complete sector
 * @param tg Track generator
 * @param sector Sector parameters
 * @param format Format specification
 * @return 0 on success
 */
int uft_trkgen_write_sector(uft_trkgen_t *tg, const uft_trkgen_sector_t *sector,
                            const uft_trkgen_format_t *format);

/**
 * @brief Fill remainder of track with gap
 * @param tg Track generator
 * @param target_bits Target track length in bits (0 = calculate from RPM)
 */
void uft_trkgen_fill_track(uft_trkgen_t *tg, size_t target_bits);

/*===========================================================================
 * CRC Operations
 *===========================================================================*/

/**
 * @brief Start CRC calculation
 * @param tg Track generator
 */
void uft_trkgen_crc_start(uft_trkgen_t *tg);

/**
 * @brief Write accumulated CRC
 * @param tg Track generator
 */
void uft_trkgen_crc_write(uft_trkgen_t *tg);

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Calculate track length in bits
 * @param bitrate Bit rate
 * @param rpm Drive RPM
 * @return Track length in bits
 */
static inline size_t uft_trkgen_track_bits(uint32_t bitrate, uint16_t rpm)
{
    return (size_t)((uint64_t)bitrate * 60 / rpm);
}

/**
 * @brief Get current bit position
 * @param tg Track generator
 * @return Current bit index
 */
static inline size_t uft_trkgen_get_position(const uft_trkgen_t *tg)
{
    return tg->bit_index;
}

/**
 * @brief Get remaining bits in track
 * @param tg Track generator
 * @return Remaining bits
 */
static inline size_t uft_trkgen_remaining(const uft_trkgen_t *tg)
{
    size_t total = uft_trkgen_track_bits(tg->bitrate, tg->rpm);
    return (tg->bit_index < total) ? (total - tg->bit_index) : 0;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_TRACK_GENERATOR_H */
