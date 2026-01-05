/**
 * @file uft_sector_extractor.h
 * @brief Sector Extraction from Raw Tracks
 * 
 * Based on HxCFloppyEmulator sector_extractor.c
 * Copyright (C) 2006-2025 Jean-François DEL NERO
 * License: GPL-2.0+
 * 
 * Extracts sectors from raw MFM/FM/GCR encoded track data:
 * - Finds sync patterns and address marks
 * - Decodes sector headers (IDAM)
 * - Extracts and verifies sector data (DAM)
 * - Supports multiple encoding schemes
 */

#ifndef UFT_SECTOR_EXTRACTOR_H
#define UFT_SECTOR_EXTRACTOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Maximum sectors per track */
#define UFT_SECEXT_MAX_SECTORS      64

/** Maximum sector size */
#define UFT_SECEXT_MAX_SECTOR_SIZE  8192

/** Search window for DAM after IDAM (in bits) */
#define UFT_SECEXT_DAM_SEARCH_BITS  1024

/*===========================================================================
 * Encoding Types
 *===========================================================================*/

/**
 * @brief Encoding detection result
 */
typedef enum {
    UFT_SECEXT_ENC_UNKNOWN = 0,
    UFT_SECEXT_ENC_FM,          /**< FM (single density) */
    UFT_SECEXT_ENC_MFM,         /**< MFM (double/high density) */
    UFT_SECEXT_ENC_GCR_C64,     /**< Commodore 64 GCR */
    UFT_SECEXT_ENC_GCR_APPLE2,  /**< Apple II 6-and-2 GCR */
    UFT_SECEXT_ENC_GCR_MAC,     /**< Apple Macintosh GCR */
    UFT_SECEXT_ENC_GCR_VICTOR,  /**< Victor 9000 GCR */
    UFT_SECEXT_ENC_AMIGA        /**< Amiga MFM */
} uft_secext_encoding_t;

/**
 * @brief Sector status flags
 */
typedef enum {
    UFT_SECEXT_STATUS_OK           = 0x00,  /**< Sector OK */
    UFT_SECEXT_STATUS_NO_IDAM      = 0x01,  /**< No ID field found */
    UFT_SECEXT_STATUS_IDAM_CRC_ERR = 0x02,  /**< ID field CRC error */
    UFT_SECEXT_STATUS_NO_DAM       = 0x04,  /**< No data field found */
    UFT_SECEXT_STATUS_DATA_CRC_ERR = 0x08,  /**< Data CRC error */
    UFT_SECEXT_STATUS_DELETED      = 0x10,  /**< Deleted data mark */
    UFT_SECEXT_STATUS_WEAK         = 0x20,  /**< Weak/fuzzy bits detected */
    UFT_SECEXT_STATUS_DUPLICATE    = 0x40   /**< Duplicate sector ID */
} uft_secext_status_t;

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/**
 * @brief Extracted sector information
 */
typedef struct {
    /* ID field (IDAM) */
    uint8_t cylinder;           /**< Cylinder from IDAM */
    uint8_t head;               /**< Head from IDAM */
    uint8_t sector;             /**< Sector number from IDAM */
    uint8_t size_code;          /**< Size code from IDAM */
    uint16_t idam_crc;          /**< IDAM CRC (read) */
    uint16_t idam_crc_calc;     /**< IDAM CRC (calculated) */
    
    /* Data field (DAM) */
    uint8_t *data;              /**< Sector data (caller allocates) */
    size_t data_size;           /**< Actual data size */
    uint16_t data_crc;          /**< Data CRC (read) */
    uint16_t data_crc_calc;     /**< Data CRC (calculated) */
    
    /* Metadata */
    uft_secext_status_t status; /**< Extraction status */
    size_t idam_bit_offset;     /**< Bit offset of IDAM in track */
    size_t dam_bit_offset;      /**< Bit offset of DAM in track */
    size_t idam_dam_gap;        /**< Bits between IDAM and DAM */
    
    /* Timing info (for weak bit detection) */
    double avg_cell_time;       /**< Average bit cell time */
    double cell_time_variance;  /**< Bit cell time variance */
} uft_secext_sector_t;

/**
 * @brief Track extraction result
 */
typedef struct {
    uft_secext_encoding_t encoding;     /**< Detected encoding */
    uint8_t physical_track;             /**< Physical track number */
    uint8_t physical_head;              /**< Physical head number */
    
    uft_secext_sector_t sectors[UFT_SECEXT_MAX_SECTORS];
    size_t sector_count;                /**< Number of sectors found */
    
    size_t track_length_bits;           /**< Total track length */
    size_t index_to_index_bits;         /**< Index pulse to index pulse */
    
    /* Statistics */
    size_t good_sectors;                /**< Sectors with no errors */
    size_t crc_errors;                  /**< Sectors with CRC errors */
    size_t missing_data;                /**< IDAMs without DAM */
    size_t duplicates;                  /**< Duplicate sector IDs */
} uft_secext_track_t;

/**
 * @brief Extractor configuration
 */
typedef struct {
    uft_secext_encoding_t encoding;     /**< Force encoding (0=auto) */
    uint32_t bit_rate;                  /**< Expected bit rate (0=auto) */
    
    bool allow_crc_errors;              /**< Extract sectors with CRC errors */
    bool detect_weak_bits;              /**< Analyze for weak/fuzzy bits */
    bool extract_deleted;               /**< Extract deleted sectors */
    
    size_t max_sectors;                 /**< Maximum sectors to extract */
    size_t dam_search_window;           /**< DAM search window (bits) */
} uft_secext_config_t;

/*===========================================================================
 * Initialization
 *===========================================================================*/

/**
 * @brief Initialize extractor configuration with defaults
 * @param config Configuration to initialize
 */
void uft_secext_config_default(uft_secext_config_t *config);

/**
 * @brief Initialize track result structure
 * @param track Track structure to initialize
 */
void uft_secext_track_init(uft_secext_track_t *track);

/**
 * @brief Free track result resources
 * @param track Track structure to free
 */
void uft_secext_track_free(uft_secext_track_t *track);

/*===========================================================================
 * Extraction Functions
 *===========================================================================*/

/**
 * @brief Extract sectors from MFM track
 * @param bitstream Raw MFM bitstream
 * @param bit_count Number of bits
 * @param config Extraction configuration
 * @param track Output track result
 * @return Number of sectors extracted
 */
int uft_secext_extract_mfm(const uint8_t *bitstream, size_t bit_count,
                           const uft_secext_config_t *config,
                           uft_secext_track_t *track);

/**
 * @brief Extract sectors from FM track
 * @param bitstream Raw FM bitstream
 * @param bit_count Number of bits
 * @param config Extraction configuration
 * @param track Output track result
 * @return Number of sectors extracted
 */
int uft_secext_extract_fm(const uint8_t *bitstream, size_t bit_count,
                          const uft_secext_config_t *config,
                          uft_secext_track_t *track);

/**
 * @brief Extract sectors from C64 GCR track
 * @param bitstream Raw GCR bitstream
 * @param bit_count Number of bits
 * @param config Extraction configuration
 * @param track Output track result
 * @return Number of sectors extracted
 */
int uft_secext_extract_gcr_c64(const uint8_t *bitstream, size_t bit_count,
                               const uft_secext_config_t *config,
                               uft_secext_track_t *track);

/**
 * @brief Extract sectors from Amiga MFM track
 * @param bitstream Raw MFM bitstream
 * @param bit_count Number of bits
 * @param config Extraction configuration
 * @param track Output track result
 * @return Number of sectors extracted
 */
int uft_secext_extract_amiga(const uint8_t *bitstream, size_t bit_count,
                             const uft_secext_config_t *config,
                             uft_secext_track_t *track);

/**
 * @brief Auto-detect encoding and extract sectors
 * @param bitstream Raw bitstream
 * @param bit_count Number of bits
 * @param config Extraction configuration
 * @param track Output track result
 * @return Number of sectors extracted, or -1 on error
 */
int uft_secext_extract_auto(const uint8_t *bitstream, size_t bit_count,
                            const uft_secext_config_t *config,
                            uft_secext_track_t *track);

/*===========================================================================
 * Pattern Search Functions
 *===========================================================================*/

/**
 * @brief Find MFM A1 sync marks
 * @param bitstream Bitstream to search
 * @param bit_count Number of bits
 * @param positions Output array of bit positions
 * @param max_positions Maximum positions to find
 * @return Number of sync marks found
 */
size_t uft_secext_find_a1_sync(const uint8_t *bitstream, size_t bit_count,
                               size_t *positions, size_t max_positions);

/**
 * @brief Find FM sync marks
 * @param bitstream Bitstream to search
 * @param bit_count Number of bits
 * @param positions Output array of bit positions
 * @param max_positions Maximum positions to find
 * @return Number of sync marks found
 */
size_t uft_secext_find_fm_sync(const uint8_t *bitstream, size_t bit_count,
                               size_t *positions, size_t max_positions);

/**
 * @brief Find C64 GCR sync marks
 * @param bitstream Bitstream to search
 * @param bit_count Number of bits
 * @param positions Output array of bit positions
 * @param max_positions Maximum positions to find
 * @return Number of sync marks found
 */
size_t uft_secext_find_gcr_sync(const uint8_t *bitstream, size_t bit_count,
                                size_t *positions, size_t max_positions);

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Detect track encoding
 * @param bitstream Raw bitstream
 * @param bit_count Number of bits
 * @return Detected encoding type
 */
uft_secext_encoding_t uft_secext_detect_encoding(const uint8_t *bitstream,
                                                  size_t bit_count);

/**
 * @brief Get sector size from size code
 * @param size_code Size code (0-7)
 * @return Sector size in bytes
 */
static inline size_t uft_secext_size_from_code(uint8_t size_code)
{
    return 128u << (size_code & 0x07);
}

/**
 * @brief Get encoding name
 * @param encoding Encoding type
 * @return Human-readable name
 */
const char *uft_secext_encoding_name(uft_secext_encoding_t encoding);

/**
 * @brief Get status description
 * @param status Status flags
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Pointer to buffer
 */
char *uft_secext_status_string(uft_secext_status_t status,
                               char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SECTOR_EXTRACTOR_H */
