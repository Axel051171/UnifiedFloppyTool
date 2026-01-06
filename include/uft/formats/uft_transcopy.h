/**
 * @file uft_transcopy.h
 * @brief Transcopy (.tc) Disk Image Format Support
 * @version 1.0.0
 * 
 * Transcopy is a preservation format that stores raw bitstream data
 * with per-track timing and weak bit information.
 * 
 * Supported disk types:
 *   - MFM High Density (1.44MB)
 *   - MFM Double Density (720K, 360K)
 *   - FM Single Density
 *   - Apple II GCR
 *   - Commodore GCR (1541)
 *   - Commodore Amiga MFM
 *   - Atari FM
 * 
 * Format structure:
 *   0x000-0x001: Signature "TC"
 *   0x002-0x021: Comment (32 bytes)
 *   0x022-0x0FF: Comment2 + reserved
 *   0x100:       Disk type
 *   0x101:       Track start
 *   0x102:       Track end  
 *   0x103:       Sides (1 or 2)
 *   0x104:       Track increment
 *   0x105-0x304: Skew table (512 bytes)
 *   0x305-0x504: Offset table (512 bytes, LE uint16[256])
 *   0x505-0x704: Length table (512 bytes, LE uint16[256])
 *   0x705-0x904: Flags table (512 bytes)
 *   0x905-0x3FFF: Timing table + reserved
 *   0x4000+:     Track data
 * 
 * References:
 *   - DiskImageTool (VB.NET, MIT License)
 *   - Central Point Software documentation
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_TRANSCOPY_H
#define UFT_TRANSCOPY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** File signature */
#define UFT_TC_SIGNATURE        "TC"
#define UFT_TC_SIGNATURE_LEN    2

/** Header offsets */
#define UFT_TC_OFF_SIGNATURE    0x000
#define UFT_TC_OFF_COMMENT      0x002
#define UFT_TC_OFF_COMMENT2     0x022
#define UFT_TC_OFF_DISKTYPE     0x100
#define UFT_TC_OFF_TRACK_START  0x101
#define UFT_TC_OFF_TRACK_END    0x102
#define UFT_TC_OFF_SIDES        0x103
#define UFT_TC_OFF_TRACK_INC    0x104
#define UFT_TC_OFF_SKEWS        0x105
#define UFT_TC_OFF_OFFSETS      0x305
#define UFT_TC_OFF_LENGTHS      0x505
#define UFT_TC_OFF_FLAGS        0x705
#define UFT_TC_OFF_TIMINGS      0x905
#define UFT_TC_OFF_DATA         0x4000

/** Field sizes */
#define UFT_TC_COMMENT_LEN      32
#define UFT_TC_MAX_TRACKS       256
#define UFT_TC_HEADER_SIZE      0x4000

/** Track flags */
#define UFT_TC_FLAG_KEEP_LENGTH     0x01
#define UFT_TC_FLAG_COPY_INDEX      0x02
#define UFT_TC_FLAG_COPY_WEAK       0x04
#define UFT_TC_FLAG_VERIFY_WRITE    0x08
#define UFT_TC_FLAG_LEN_TOLERANCE   0x20
#define UFT_TC_FLAG_NO_ADDR_MARKS   0x80

/* ═══════════════════════════════════════════════════════════════════════════
 * Disk Types
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Transcopy disk type enumeration
 */
typedef enum uft_tc_disk_type {
    UFT_TC_TYPE_UNKNOWN        = 0xFF,
    UFT_TC_TYPE_MFM_HD         = 0x02,  /**< MFM High Density (1.44MB) */
    UFT_TC_TYPE_MFM_DD_360     = 0x03,  /**< MFM Double Density 360 RPM */
    UFT_TC_TYPE_APPLE_GCR      = 0x04,  /**< Apple II GCR */
    UFT_TC_TYPE_FM_SD          = 0x05,  /**< FM Single Density */
    UFT_TC_TYPE_C64_GCR        = 0x06,  /**< Commodore 64 GCR */
    UFT_TC_TYPE_MFM_DD         = 0x07,  /**< MFM Double Density (720KB) */
    UFT_TC_TYPE_AMIGA_MFM      = 0x08,  /**< Commodore Amiga MFM */
    UFT_TC_TYPE_ATARI_FM       = 0x0C   /**< Atari FM */
} uft_tc_disk_type_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Status Codes
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum uft_tc_status {
    UFT_TC_OK = 0,              /**< Success */
    UFT_TC_E_INVALID = 1,       /**< Invalid parameter */
    UFT_TC_E_SIGNATURE = 2,     /**< Invalid signature */
    UFT_TC_E_TRUNC = 3,         /**< Truncated data */
    UFT_TC_E_ALLOC = 4,         /**< Memory allocation failed */
    UFT_TC_E_TRACK = 5,         /**< Invalid track number */
    UFT_TC_E_FORMAT = 6         /**< Invalid format */
} uft_tc_status_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Data Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Transcopy track information
 */
typedef struct uft_tc_track {
    uint16_t    offset;         /**< Offset in data section (x256) */
    uint16_t    length;         /**< Track length in bytes */
    uint8_t     flags;          /**< Track flags */
    uint8_t     skew;           /**< Sector skew */
    uint16_t    timing;         /**< Timing information */
    uint8_t*    data;           /**< Track data (NULL if not loaded) */
    bool        has_weak_bits;  /**< Contains weak bit markers */
} uft_tc_track_t;

/**
 * @brief Transcopy image
 */
typedef struct uft_tc_image {
    /* Header info */
    char                comment[UFT_TC_COMMENT_LEN + 1];
    char                comment2[UFT_TC_COMMENT_LEN + 1];
    uft_tc_disk_type_t  disk_type;
    uint8_t             track_start;
    uint8_t             track_end;
    uint8_t             sides;
    uint8_t             track_increment;
    
    /* Track array */
    uft_tc_track_t*     tracks;
    uint16_t            track_count;
    
    /* Raw data reference */
    const uint8_t*      raw_data;
    size_t              raw_size;
    bool                owns_data;
} uft_tc_image_t;

/**
 * @brief Transcopy writer
 */
typedef struct uft_tc_writer {
    uft_tc_disk_type_t  disk_type;
    uint8_t             track_start;
    uint8_t             track_end;
    uint8_t             sides;
    uint8_t             track_increment;
    char                comment[UFT_TC_COMMENT_LEN + 1];
    
    /* Track data */
    uft_tc_track_t*     tracks;
    uint16_t            track_count;
    
    /* Output buffer */
    uint8_t*            data;
    size_t              size;
    size_t              capacity;
} uft_tc_writer_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Detection Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Detect if data is a Transcopy image
 * 
 * @param data      File data
 * @param size      Data size
 * @return          true if Transcopy image
 */
bool uft_tc_detect(const uint8_t* data, size_t size);

/**
 * @brief Get Transcopy detection confidence (0-100)
 * 
 * @param data      File data
 * @param size      Data size
 * @return          Confidence score
 */
int uft_tc_detect_confidence(const uint8_t* data, size_t size);

/**
 * @brief Get disk type name
 * 
 * @param type      Disk type
 * @return          Type name string
 */
const char* uft_tc_disk_type_name(uft_tc_disk_type_t type);

/* ═══════════════════════════════════════════════════════════════════════════
 * Reading Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Open Transcopy image for reading
 * 
 * @param data      File data
 * @param size      Data size
 * @param image     Output image structure
 * @return          Status code
 */
uft_tc_status_t uft_tc_open(const uint8_t* data, size_t size,
                             uft_tc_image_t* image);

/**
 * @brief Load track data
 * 
 * @param image     Image structure
 * @param track     Track number (physical)
 * @param side      Side (0 or 1)
 * @return          Status code
 */
uft_tc_status_t uft_tc_load_track(uft_tc_image_t* image,
                                   uint8_t track, uint8_t side);

/**
 * @brief Get track data
 * 
 * @param image     Image structure
 * @param track     Track number
 * @param side      Side
 * @param data      Output data pointer
 * @param length    Output data length
 * @return          Status code
 */
uft_tc_status_t uft_tc_get_track(const uft_tc_image_t* image,
                                  uint8_t track, uint8_t side,
                                  const uint8_t** data, size_t* length);

/**
 * @brief Get track flags
 * 
 * @param image     Image structure
 * @param track     Track number
 * @param side      Side
 * @return          Track flags or 0 on error
 */
uint8_t uft_tc_get_track_flags(const uft_tc_image_t* image,
                                uint8_t track, uint8_t side);

/**
 * @brief Free image resources
 * 
 * @param image     Image to free
 */
void uft_tc_close(uft_tc_image_t* image);

/* ═══════════════════════════════════════════════════════════════════════════
 * Writing Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize Transcopy writer
 * 
 * @param writer    Writer to initialize
 * @param disk_type Disk type
 * @param tracks    Number of tracks
 * @param sides     Number of sides (1 or 2)
 * @return          Status code
 */
uft_tc_status_t uft_tc_writer_init(uft_tc_writer_t* writer,
                                    uft_tc_disk_type_t disk_type,
                                    uint8_t tracks, uint8_t sides);

/**
 * @brief Set comment
 * 
 * @param writer    Writer
 * @param comment   Comment string (max 32 chars)
 */
void uft_tc_writer_set_comment(uft_tc_writer_t* writer, const char* comment);

/**
 * @brief Add track data
 * 
 * @param writer    Writer
 * @param track     Track number
 * @param side      Side
 * @param data      Track data
 * @param length    Data length
 * @param flags     Track flags
 * @return          Status code
 */
uft_tc_status_t uft_tc_writer_add_track(uft_tc_writer_t* writer,
                                         uint8_t track, uint8_t side,
                                         const uint8_t* data, size_t length,
                                         uint8_t flags);

/**
 * @brief Finalize and get output
 * 
 * @param writer    Writer
 * @param out_data  Output data (caller must free)
 * @param out_size  Output size
 * @return          Status code
 */
uft_tc_status_t uft_tc_writer_finish(uft_tc_writer_t* writer,
                                      uint8_t** out_data, size_t* out_size);

/**
 * @brief Free writer resources
 * 
 * @param writer    Writer to free
 */
void uft_tc_writer_free(uft_tc_writer_t* writer);

/* ═══════════════════════════════════════════════════════════════════════════
 * Conversion Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get encoding type for disk type
 * 
 * @param disk_type Transcopy disk type
 * @return          Encoding identifier (0=unknown, 1=MFM, 2=FM, 3=GCR)
 */
int uft_tc_get_encoding(uft_tc_disk_type_t disk_type);

/**
 * @brief Get expected track length for disk type
 * 
 * @param disk_type Transcopy disk type
 * @param track     Track number (for variable-density formats)
 * @return          Expected raw track length in bytes
 */
size_t uft_tc_expected_track_length(uft_tc_disk_type_t disk_type, uint8_t track);

/**
 * @brief Check if disk type uses variable density
 * 
 * @param disk_type Transcopy disk type
 * @return          true if variable density
 */
bool uft_tc_is_variable_density(uft_tc_disk_type_t disk_type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TRANSCOPY_H */
