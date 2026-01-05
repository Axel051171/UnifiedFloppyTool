/**
 * @file uft_mfm.h
 * @brief MFM/FM Decoder for IBM PC and compatible disk formats
 * 
 * Implements complete Flux→MFM→Sector decoding for PC disk formats.
 * Supports FM (Single Density), MFM (Double Density), and M2FM variants.
 * 
 * Encoding Types:
 * - FM:   Frequency Modulation (1 data bit → 2 coded bits with clock)
 * - MFM:  Modified FM (clock only between 0-0 data transitions)
 * - M2FM: Modified MFM (rare variant)
 * 
 * IBM Format Structure:
 * - Index pulse marks track start
 * - Gap 0: Post-index gap
 * - Sectors: Each with IDAM (header) + Gap + DAM (data)
 * - Gap 4: End-of-track gap
 * 
 * Standard PC Geometries:
 * - 3.5" DD:  80 tracks, 2 heads,  9 SPT, 512B → 720KB
 * - 3.5" HD:  80 tracks, 2 heads, 18 SPT, 512B → 1.44MB
 * - 5.25" DD: 40 tracks, 2 heads,  9 SPT, 512B → 360KB
 * - 5.25" HD: 80 tracks, 2 heads, 15 SPT, 512B → 1.2MB
 * 
 * @version 2.10.0
 * @date 2024-12-27
 */

#ifndef UFT_MFM_H
#define UFT_MFM_H

#include "uft/uft_error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Encoding type
 */
typedef enum {
    UFT_ENCODING_FM = 0,      /**< FM - Single Density */
    UFT_ENCODING_MFM = 1,     /**< MFM - Double Density (standard) */
    UFT_ENCODING_M2FM = 2     /**< M2FM - Modified MFM variant */
} uft_mfm_encoding_t;

/**
 * @brief Standard PC disk formats
 */
typedef enum {
    UFT_FORMAT_PC_360K = 0,   /**< 5.25" DD - 40T, 2H, 9SPT, 512B */
    UFT_FORMAT_PC_720K,       /**< 3.5"  DD - 80T, 2H, 9SPT, 512B */
    UFT_FORMAT_PC_1200K,      /**< 5.25" HD - 80T, 2H, 15SPT, 512B */
    UFT_FORMAT_PC_1440K,      /**< 3.5"  HD - 80T, 2H, 18SPT, 512B */
    UFT_FORMAT_PC_2880K,      /**< 3.5"  ED - 80T, 2H, 36SPT, 512B */
    UFT_FORMAT_CUSTOM         /**< User-defined geometry */
} uft_mfm_format_t;

/**
 * @brief Disk geometry
 */
typedef struct {
    uint8_t cylinders;        /**< Tracks per side */
    uint8_t heads;            /**< Number of heads */
    uint8_t sectors_per_track;/**< Sectors per track */
    uint16_t sector_size;     /**< Bytes per sector (usually 512) */
    uint32_t bitrate;         /**< Bitrate in bits/second */
    uint32_t rpm;             /**< Spindle RPM (300 or 360) */
} uft_mfm_geometry_t;

/**
 * @brief Standard PC geometries (predefined)
 */
extern const uft_mfm_geometry_t uft_mfm_formats[];

/**
 * @brief IBM Address Mark types
 */
typedef enum {
    UFT_AM_IDAM = 0xFE,       /**< ID Address Mark (sector header) */
    UFT_AM_DAM = 0xFB,        /**< Data Address Mark (normal data) */
    UFT_AM_DDAM = 0xF8,       /**< Deleted Data Address Mark */
    UFT_AM_IAM = 0xFC         /**< Index Address Mark (track start) */
} uft_mfm_address_mark_t;

/**
 * @brief IBM sector ID (IDAM)
 */
typedef struct {
    uint8_t cylinder;         /**< C - Cylinder number */
    uint8_t head;             /**< H - Head number */
    uint8_t sector;           /**< R - Sector number (usually 1-based) */
    uint8_t size_code;        /**< N - Size code (0=128, 1=256, 2=512, 3=1024) */
    uint16_t crc;             /**< CRC-16-CCITT */
} uft_mfm_idam_t;

/**
 * @brief Sector data block
 */
typedef struct {
    uint8_t* data;            /**< Sector data (malloc'd) */
    uint16_t size;            /**< Data size in bytes */
    uint16_t crc;             /**< CRC-16-CCITT */
    bool deleted;             /**< Deleted data mark (DDAM) */
} uft_mfm_sector_data_t;

/**
 * @brief Complete decoded sector
 */
typedef struct {
    uft_mfm_idam_t id;        /**< Sector ID (IDAM) */
    uft_mfm_sector_data_t data; /**< Sector data (DAM/DDAM) */
    bool id_valid;            /**< IDAM CRC OK */
    bool data_valid;          /**< DAM CRC OK */
    uint32_t bit_offset;      /**< Bit offset in track (for analysis) */
} uft_mfm_sector_t;

/**
 * @brief Decoded track
 */
typedef struct {
    uint8_t cylinder;         /**< Physical cylinder */
    uint8_t head;             /**< Physical head */
    uint8_t sectors_found;    /**< Number of sectors decoded */
    uft_mfm_sector_t sectors[256]; /**< Decoded sectors (max 256) */
    uint32_t index_pulse_pos; /**< Index pulse bit position */
    uint32_t bitstream_length;/**< Total bits in track */
} uft_mfm_track_t;

/**
 * @brief MFM decoder context
 */
typedef struct {
    /** Encoding type */
    uft_mfm_encoding_t encoding;
    
    /** Geometry */
    uft_mfm_geometry_t geometry;
    
    /** Timing */
    uint32_t nominal_cell_ns;
    uint32_t tolerance_ns;
    
    /** Statistics */
    uint32_t total_sectors_decoded;
    uint32_t crc_errors_id;
    uint32_t crc_errors_data;
    uint32_t address_marks_found;
    
    /** Error context */
    uft_error_ctx_t error;
} uft_mfm_ctx_t;

/**
 * @brief Create MFM decoder context
 */
uft_rc_t uft_mfm_create(uft_mfm_ctx_t** ctx);

/**
 * @brief Destroy MFM decoder context
 */
void uft_mfm_destroy(uft_mfm_ctx_t** ctx);

/**
 * @brief Set format (applies standard geometry)
 */
uft_rc_t uft_mfm_set_format(uft_mfm_ctx_t* ctx, uft_mfm_format_t format);

/**
 * @brief Set custom geometry
 */
uft_rc_t uft_mfm_set_geometry(uft_mfm_ctx_t* ctx, const uft_mfm_geometry_t* geom);

/**
 * @brief Decode MFM bit pair to data bit
 * 
 * MFM encoding: Clock bit | Data bit
 * Clock rules:
 * - Clock = 1 if previous data = 0 AND current data = 0
 * - Clock = 0 otherwise
 * 
 * @param mfm_bits 2 MFM bits (clock + data)
 * @param[out] data_bit Decoded data bit
 * @param[in,out] prev_bit Previous data bit (for clock generation)
 * @return UFT_SUCCESS or UFT_ERR_CORRUPTED
 */
uft_rc_t uft_mfm_decode_bit(uint8_t mfm_bits, bool* data_bit, bool* prev_bit);

/**
 * @brief Decode FM bit pair to data bit
 * 
 * FM encoding: Always Clock = 1, then Data bit
 * 
 * @param fm_bits 2 FM bits
 * @param[out] data_bit Decoded data bit
 * @return UFT_SUCCESS or UFT_ERR_CORRUPTED
 */
uft_rc_t uft_fm_decode_bit(uint8_t fm_bits, bool* data_bit);

/**
 * @brief Decode MFM byte (16 MFM bits → 8 data bits)
 */
uft_rc_t uft_mfm_decode_byte(const uint8_t* mfm_bits, uint8_t* data_byte, bool* prev_bit);

/**
 * @brief Calculate CRC-16-CCITT
 * 
 * Polynomial: 0x1021 (x^16 + x^12 + x^5 + 1)
 * Initial: 0xFFFF (for IBM format)
 * 
 * @param data Data buffer
 * @param len Data length
 * @param init Initial CRC value
 * @return Computed CRC
 */
uint16_t uft_mfm_crc16(const uint8_t* data, size_t len, uint16_t init);

/**
 * @brief Find address mark in MFM bitstream
 * 
 * Searches for sync pattern (0x00 bytes) + address mark byte.
 * 
 * Standard sync patterns:
 * - IDAM: 12x 0x00 + 3x 0xA1* + 0xFE
 * - DAM:  12x 0x00 + 3x 0xA1* + 0xFB/0xF8
 * 
 * (*0xA1 with missing clock bit - MFM only)
 * 
 * @param bitstream MFM bitstream
 * @param bit_count Bitstream length
 * @param start_bit Start search position
 * @param[out] mark_type Address mark type found
 * @param[out] mark_pos Bit position after sync (at mark byte)
 * @return UFT_SUCCESS if found
 */
uft_rc_t uft_mfm_find_address_mark(
    const uint8_t* bitstream,
    uint32_t bit_count,
    uint32_t start_bit,
    uft_mfm_address_mark_t* mark_type,
    uint32_t* mark_pos
);

/**
 * @brief Decode IDAM (sector ID)
 * 
 * Expects bitstream positioned after 0xFE mark.
 * Reads: C H R N CRC(2 bytes)
 * 
 * @param bitstream Bitstream at IDAM start
 * @param[out] idam Decoded sector ID
 * @return UFT_SUCCESS or UFT_ERR_CRC
 */
uft_rc_t uft_mfm_decode_idam(
    const uint8_t* bitstream,
    uft_mfm_idam_t* idam
);

/**
 * @brief Decode DAM (sector data)
 * 
 * Expects bitstream positioned after 0xFB/0xF8 mark.
 * Reads: Data[size] + CRC(2 bytes)
 * 
 * @param bitstream Bitstream at DAM start
 * @param size Data size in bytes (from IDAM size_code)
 * @param deleted true if DDAM (0xF8), false if normal DAM (0xFB)
 * @param[out] data Decoded sector data (malloc'd)
 * @return UFT_SUCCESS or UFT_ERR_CRC
 */
uft_rc_t uft_mfm_decode_dam(
    const uint8_t* bitstream,
    uint16_t size,
    bool deleted,
    uft_mfm_sector_data_t* data
);

/**
 * @brief Decode complete MFM track
 * 
 * Primary decode function - extracts all sectors.
 * 
 * @param[in] ctx MFM context
 * @param cylinder Cylinder number
 * @param head Head number
 * @param bitstream MFM bitstream
 * @param bit_count Bitstream length
 * @param[out] track Decoded track with sectors
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_mfm_decode_track(
    uft_mfm_ctx_t* ctx,
    uint8_t cylinder,
    uint8_t head,
    const uint8_t* bitstream,
    uint32_t bit_count,
    uft_mfm_track_t* track
);

/**
 * @brief Convert flux transitions to MFM bitstream
 * 
 * @param[in] ctx MFM context
 * @param flux_ns Flux transition times (nanoseconds)
 * @param flux_count Number of transitions
 * @param[out] bitstream Output MFM bitstream
 * @param bitstream_size Buffer size
 * @param[out] bits_decoded Number of bits decoded
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_mfm_flux_to_bitstream(
    uft_mfm_ctx_t* ctx,
    const uint32_t* flux_ns,
    uint32_t flux_count,
    uint8_t* bitstream,
    size_t bitstream_size,
    uint32_t* bits_decoded
);

/**
 * @brief Complete pipeline: Flux → IMG sectors
 * 
 * High-level function combining flux decode + MFM decode.
 * 
 * @param[in] ctx MFM context
 * @param cylinder Cylinder number
 * @param head Head number
 * @param flux_ns Flux timing data
 * @param flux_count Flux transition count
 * @param[out] track Decoded track
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_mfm_decode_track_from_flux(
    uft_mfm_ctx_t* ctx,
    uint8_t cylinder,
    uint8_t head,
    const uint32_t* flux_ns,
    uint32_t flux_count,
    uft_mfm_track_t* track
);

/**
 * @brief Get sector size from size code
 * 
 * IBM size codes: 0=128, 1=256, 2=512, 3=1024, 4=2048, etc.
 * 
 * @param size_code Size code (0-7)
 * @return Sector size in bytes
 */
static inline uint16_t uft_mfm_sector_size(uint8_t size_code) {
    return (size_code < 8) ? (128 << size_code) : 0;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_MFM_H */
