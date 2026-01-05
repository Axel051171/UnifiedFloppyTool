/**
 * @file uft_mfm.h
 * @brief MFM (Modified Frequency Modulation) encoding/decoding
 * 
 * This module provides MFM encoding and decoding support for:
 * - IBM PC floppy formats (360K, 720K, 1.2M, 1.44M, 2.88M)
 * - Amiga DD/HD formats (880K, 1.76M)
 * 
 * MFM is a flux-based encoding where:
 * - A '1' bit is encoded as a flux transition in the middle of the bit cell
 * - A '0' bit has no transition in the middle, but may have a clock transition
 *   at the start of the bit cell if the previous bit was also '0'
 * 
 * This creates the rule: no more than 3 consecutive flux intervals without
 * a transition, and no transitions closer than 2 flux intervals.
 */

#ifndef UFT_MFM_H
#define UFT_MFM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** MFM encoding: 1 data bit -> 2 flux bits */
#define UFT_MFM_BITS_IN     1
#define UFT_MFM_BITS_OUT    2

/** Standard sector sizes */
#define UFT_MFM_SECTOR_128   128
#define UFT_MFM_SECTOR_256   256
#define UFT_MFM_SECTOR_512   512
#define UFT_MFM_SECTOR_1024  1024

/** IBM sync patterns */
#define UFT_MFM_SYNC_A1      0xA1    /**< Special sync byte (missing clock) */
#define UFT_MFM_SYNC_C2      0xC2    /**< Index address mark sync */
#define UFT_MFM_GAP_BYTE     0x4E    /**< Gap filler byte */
#define UFT_MFM_FILL_BYTE    0x00    /**< Pre-sync filler */

/** IBM address marks (after sync bytes) */
#define UFT_MFM_AM_INDEX     0xFC    /**< Index address mark */
#define UFT_MFM_AM_ID        0xFE    /**< ID address mark */
#define UFT_MFM_AM_DATA      0xFB    /**< Data address mark */
#define UFT_MFM_AM_DELETED   0xF8    /**< Deleted data address mark */

/** Amiga sync word */
#define UFT_AMIGA_SYNC       0x4489  /**< MFM encoded 0xA1 with clock */
#define UFT_AMIGA_SYNC_WORD  0x44894489  /**< Two sync words */

/** CRC-CCITT polynomial */
#define UFT_CRC_CCITT_POLY   0x1021
#define UFT_CRC_CCITT_INIT   0xFFFF

/** Error codes */
typedef enum {
    UFT_MFM_OK = 0,
    UFT_MFM_ERR_SYNC_NOT_FOUND,   /**< Sync pattern not found */
    UFT_MFM_ERR_CRC,              /**< CRC mismatch */
    UFT_MFM_ERR_ID_NOT_FOUND,     /**< Sector ID not found */
    UFT_MFM_ERR_DATA_NOT_FOUND,   /**< Data field not found */
    UFT_MFM_ERR_BUFFER_TOO_SMALL, /**< Output buffer too small */
    UFT_MFM_ERR_INVALID_PARAM,    /**< Invalid parameter */
    UFT_MFM_ERR_DELETED_DATA,     /**< Deleted data mark found */
} uft_mfm_error_t;

/* ============================================================================
 * MFM Encoding/Decoding
 * ============================================================================ */

/**
 * @brief Encode data to MFM bit stream
 * 
 * Encodes raw data bytes into MFM flux representation.
 * Each input byte becomes 16 flux bits (2 bytes output).
 * 
 * @param data      Input data buffer
 * @param data_len  Length of input data
 * @param mfm_out   Output MFM buffer (must be 2x data_len)
 * @param last_bit  Last bit of previous byte (for clock generation)
 * @return UFT_MFM_OK on success
 */
uft_mfm_error_t uft_mfm_encode(
    const uint8_t *data,
    size_t data_len,
    uint8_t *mfm_out,
    uint8_t last_bit
);

/**
 * @brief Decode MFM bit stream to data
 * 
 * Decodes MFM flux representation back to raw data.
 * Each 16 flux bits (2 input bytes) becomes 1 data byte.
 * 
 * @param mfm_in    Input MFM buffer
 * @param mfm_len   Length of MFM buffer
 * @param data_out  Output data buffer (must be mfm_len/2)
 * @return UFT_MFM_OK on success
 */
uft_mfm_error_t uft_mfm_decode(
    const uint8_t *mfm_in,
    size_t mfm_len,
    uint8_t *data_out
);

/**
 * @brief Encode a special sync byte (A1 with missing clock)
 * 
 * The sync byte A1 is encoded with a missing clock bit to create
 * a unique pattern that cannot occur in normal data.
 * 
 * @return MFM encoded sync word (16 bits)
 */
static inline uint16_t uft_mfm_encode_sync_a1(void) {
    return 0x4489;  /* A1 with missing clock at bit 5 */
}

/**
 * @brief Encode a byte to MFM (single byte version)
 * 
 * @param data      Data byte to encode
 * @param last_bit  Last bit of previous byte
 * @return MFM encoded word (16 bits, MSB first)
 */
uint16_t uft_mfm_encode_byte(uint8_t data, uint8_t last_bit);

/**
 * @brief Decode MFM word to byte
 * 
 * @param mfm MFM encoded word (16 bits)
 * @return Decoded data byte
 */
uint8_t uft_mfm_decode_byte(uint16_t mfm);

/* ============================================================================
 * CRC Calculation
 * ============================================================================ */

/**
 * @brief Calculate CRC-CCITT
 * 
 * Calculates the CRC-CCITT checksum used by IBM MFM formats.
 * Polynomial: x^16 + x^12 + x^5 + 1 (0x1021)
 * Initial value: 0xFFFF
 * 
 * @param data      Input data buffer
 * @param len       Length of data
 * @param init_crc  Initial CRC value (usually 0xFFFF)
 * @return CRC-CCITT checksum
 */
uint16_t uft_mfm_crc_ccitt(const uint8_t *data, size_t len, uint16_t init_crc);

/**
 * @brief Update CRC with single byte
 * 
 * @param crc   Current CRC value
 * @param byte  Data byte
 * @return Updated CRC
 */
uint16_t uft_mfm_crc_update(uint16_t crc, uint8_t byte);

/* ============================================================================
 * IBM Sector Encoding/Decoding
 * ============================================================================ */

/**
 * @brief IBM sector ID field
 */
typedef struct {
    uint8_t cylinder;       /**< Cylinder/track number */
    uint8_t head;           /**< Head/side number */
    uint8_t sector;         /**< Sector number (1-based) */
    uint8_t size_code;      /**< Size code: 0=128, 1=256, 2=512, 3=1024 */
    uint16_t crc;           /**< CRC-CCITT of ID field */
} uft_mfm_sector_id_t;

/**
 * @brief Encode IBM sector ID field
 * 
 * Creates the complete ID field including sync bytes, address mark,
 * ID data, and CRC.
 * 
 * @param id        Sector ID parameters
 * @param mfm_out   Output buffer for MFM encoded ID field
 * @return Size of encoded field in bytes
 */
size_t uft_mfm_encode_sector_id(
    const uft_mfm_sector_id_t *id,
    uint8_t *mfm_out
);

/**
 * @brief Decode IBM sector ID field
 * 
 * Parses an MFM encoded ID field and extracts the sector parameters.
 * 
 * @param mfm_in    Input MFM buffer (positioned at sync)
 * @param id        Output sector ID structure
 * @return UFT_MFM_OK on success, UFT_MFM_ERR_CRC on checksum error
 */
uft_mfm_error_t uft_mfm_decode_sector_id(
    const uint8_t *mfm_in,
    uft_mfm_sector_id_t *id
);

/**
 * @brief Encode IBM sector data field
 * 
 * Creates the complete data field including sync bytes, address mark,
 * data, and CRC.
 * 
 * @param data          Sector data
 * @param data_len      Length of data (128, 256, 512, or 1024)
 * @param deleted       True for deleted data mark
 * @param mfm_out       Output buffer for MFM encoded data field
 * @return Size of encoded field in bytes
 */
size_t uft_mfm_encode_sector_data(
    const uint8_t *data,
    size_t data_len,
    bool deleted,
    uint8_t *mfm_out
);

/**
 * @brief Decode IBM sector data field
 * 
 * Parses an MFM encoded data field and extracts the sector data.
 * 
 * @param mfm_in        Input MFM buffer (positioned at sync)
 * @param data_out      Output data buffer
 * @param expected_len  Expected data length
 * @param deleted       Output: true if deleted data mark found
 * @return UFT_MFM_OK on success
 */
uft_mfm_error_t uft_mfm_decode_sector_data(
    const uint8_t *mfm_in,
    uint8_t *data_out,
    size_t expected_len,
    bool *deleted
);

/* ============================================================================
 * Amiga Sector Encoding/Decoding
 * ============================================================================ */

/**
 * @brief Amiga sector header
 */
typedef struct {
    uint8_t format;         /**< Format byte (0xFF for DOS) */
    uint8_t track;          /**< Track number (0-159) */
    uint8_t sector;         /**< Sector number (0-10 or 0-21) */
    uint8_t sectors_to_gap; /**< Sectors until track gap */
    uint8_t label[16];      /**< OS recovery label */
    uint32_t header_checksum;
    uint32_t data_checksum;
} uft_amiga_sector_header_t;

/**
 * @brief Encode Amiga sector
 * 
 * Creates a complete Amiga sector including sync words, header,
 * and data with checksums.
 * 
 * @param header    Sector header information
 * @param data      Sector data (512 bytes)
 * @param mfm_out   Output buffer for MFM encoded sector
 * @return Size of encoded sector in bytes
 */
size_t uft_amiga_encode_sector(
    const uft_amiga_sector_header_t *header,
    const uint8_t *data,
    uint8_t *mfm_out
);

/**
 * @brief Decode Amiga sector
 * 
 * Parses an MFM encoded Amiga sector.
 * 
 * @param mfm_in    Input MFM buffer (positioned at sync)
 * @param header    Output header structure
 * @param data_out  Output data buffer (512 bytes)
 * @return UFT_MFM_OK on success
 */
uft_mfm_error_t uft_amiga_decode_sector(
    const uint8_t *mfm_in,
    uft_amiga_sector_header_t *header,
    uint8_t *data_out
);

/**
 * @brief Calculate Amiga checksum
 * 
 * Amiga uses XOR of all longwords in the data.
 * 
 * @param data  Data buffer (must be longword aligned)
 * @param len   Length in bytes (must be multiple of 4)
 * @return Checksum value
 */
uint32_t uft_amiga_checksum(const uint8_t *data, size_t len);

/* ============================================================================
 * Track-Level Operations
 * ============================================================================ */

/**
 * @brief IBM track format parameters
 */
typedef struct {
    uint8_t sectors;            /**< Sectors per track */
    uint8_t sector_size_code;   /**< Size code (2 = 512 bytes) */
    uint8_t gap3_length;        /**< Gap 3 length */
    uint8_t gap4_length;        /**< Gap 4 length */
    uint8_t interleave;         /**< Sector interleave */
    bool mfm;                   /**< True for MFM, false for FM */
    uint16_t data_rate;         /**< Data rate in kbps */
} uft_mfm_track_format_t;

/**
 * @brief Standard IBM floppy formats
 */
extern const uft_mfm_track_format_t uft_ibm_formats[];

/**
 * @brief Get format parameters by disk type
 * 
 * @param type  Disk type (e.g., "720K", "1.44M")
 * @return Pointer to format parameters, or NULL if unknown
 */
const uft_mfm_track_format_t* uft_mfm_get_format(const char *type);

/**
 * @brief Find MFM sync pattern in track data
 * 
 * Searches for the MFM sync pattern (A1 A1 A1) in raw track data.
 * 
 * @param track_data    Raw track data
 * @param track_len     Length of track data
 * @param start_offset  Starting offset for search
 * @return Offset of sync pattern, or -1 if not found
 */
ssize_t uft_mfm_find_sync(
    const uint8_t *track_data,
    size_t track_len,
    size_t start_offset
);

/**
 * @brief Find Amiga sync word in track data
 * 
 * @param track_data    Raw track data
 * @param track_len     Length of track data
 * @param start_offset  Starting offset for search
 * @return Offset of sync word, or -1 if not found
 */
ssize_t uft_amiga_find_sync(
    const uint8_t *track_data,
    size_t track_len,
    size_t start_offset
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MFM_H */
