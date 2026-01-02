/**
 * @file uft_teledisk.h
 * @brief TeleDisk TD0 Format Support
 * 
 * Algorithms ported from DiskImageTool by Digitoxin1
 * 
 * TeleDisk formats:
 * - TD0 1.x: LZW compression
 * - TD0 2.x: LZHUF compression
 * 
 * File structure:
 * - Header (12 bytes)
 * - Optional Comment Block
 * - Tracks and Sectors
 */

#ifndef UFT_TELEDISK_H
#define UFT_TELEDISK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * CONSTANTS
 *============================================================================*/

#define UFT_TD0_HEADER_SIZE     12
#define UFT_TD0_SIGNATURE_TD    0x4454  /* "TD" - uncompressed or LZW */
#define UFT_TD0_SIGNATURE_td    0x6474  /* "td" - LZHUF compressed */

/* LZHUF parameters (from DiskImageTool) */
#define UFT_TD0_LZHUF_N         4096    /* Ring buffer size */
#define UFT_TD0_LZHUF_F         60      /* Match length limit */
#define UFT_TD0_LZHUF_THRESHOLD 2       /* Minimum match for compression */

/* LZW parameters */
#define UFT_TD0_LZW_FIRST_CODE  256
#define UFT_TD0_LZW_MAX_CODES   4096    /* 12-bit codes */
#define UFT_TD0_LZW_MAX_BLOCK   0x1800  /* Max block size in bytes */

/*============================================================================
 * ENUMERATIONS
 *============================================================================*/

/**
 * @brief TD0 Data Rate
 */
typedef enum {
    UFT_TD0_RATE_250K   = 0,    /**< 250 Kbps (DD) */
    UFT_TD0_RATE_300K   = 1,    /**< 300 Kbps (HD 1.2MB) */
    UFT_TD0_RATE_500K   = 2,    /**< 500 Kbps (HD) */
    UFT_TD0_RATE_FM     = 128   /**< FM mode flag (bit 7) */
} uft_td0_rate_t;

/**
 * @brief TD0 Drive Type
 */
typedef enum {
    UFT_TD0_DRIVE_525_96TPI  = 0,   /**< 5.25" 96 TPI */
    UFT_TD0_DRIVE_525_48TPI  = 1,   /**< 5.25" 48 TPI */
    UFT_TD0_DRIVE_35_135TPI  = 2,   /**< 3.5" 135 TPI */
    UFT_TD0_DRIVE_8_INCH     = 3,   /**< 8" */
    UFT_TD0_DRIVE_35_HD      = 4,   /**< 3.5" HD */
    UFT_TD0_DRIVE_8_INCH_2   = 5    /**< 8" #2 */
} uft_td0_drive_t;

/**
 * @brief TD0 Stepping Rate
 */
typedef enum {
    UFT_TD0_STEP_SINGLE  = 0,      /**< Single stepping */
    UFT_TD0_STEP_DOUBLE  = 1,      /**< Double stepping */
    UFT_TD0_STEP_EVEN    = 2       /**< Even only (96 TPI in 48 TPI drive) */
} uft_td0_stepping_t;

/**
 * @brief TD0 Sector data encoding
 */
typedef enum {
    UFT_TD0_DATA_RAW     = 0,      /**< Raw sector data follows */
    UFT_TD0_DATA_REPEAT  = 1,      /**< Repeated byte pattern */
    UFT_TD0_DATA_RLE     = 2       /**< RLE compressed */
} uft_td0_data_encoding_t;

/*============================================================================
 * DATA STRUCTURES
 *============================================================================*/

/**
 * @brief TD0 file header (12 bytes)
 */
#pragma pack(push, 1)
typedef struct uft_td0_header {
    uint8_t signature[2];           /**< "TD" or "td" */
    uint8_t sequence;               /**< Volume sequence (0=first) */
    uint8_t check_sequence;         /**< Check sequence */
    uint8_t version;                /**< Version (e.g., 21 = 2.1) */
    uint8_t data_rate;              /**< Data rate and FM flag */
    uint8_t drive_type;             /**< Drive type */
    uint8_t stepping;               /**< Stepping and comment flag */
    uint8_t dos_alloc_flag;         /**< DOS allocation flag */
    uint8_t sides;                  /**< Number of sides (1 or 2) */
    uint16_t crc;                   /**< CRC-16 of header (little-endian) */
} uft_td0_header_t;
#pragma pack(pop)

/**
 * @brief TD0 comment block
 */
typedef struct uft_td0_comment {
    uint16_t crc;                   /**< CRC of comment data */
    uint16_t length;                /**< Length of comment */
    uint8_t year;                   /**< Year - 1900 */
    uint8_t month;                  /**< Month (1-12) */
    uint8_t day;                    /**< Day (1-31) */
    uint8_t hour;                   /**< Hour (0-23) */
    uint8_t minute;                 /**< Minute (0-59) */
    uint8_t second;                 /**< Second (0-59) */
    char* text;                     /**< Comment text (allocated) */
} uft_td0_comment_t;

/**
 * @brief TD0 track header
 */
#pragma pack(push, 1)
typedef struct uft_td0_track_header {
    uint8_t sector_count;           /**< Number of sectors (255 = end) */
    uint8_t cylinder;               /**< Physical cylinder */
    uint8_t head;                   /**< Physical head */
    uint8_t crc;                    /**< CRC-8 of track header */
} uft_td0_track_header_t;
#pragma pack(pop)

/**
 * @brief TD0 sector header
 */
#pragma pack(push, 1)
typedef struct uft_td0_sector_header {
    uint8_t cylinder;               /**< ID cylinder */
    uint8_t head;                   /**< ID head */
    uint8_t sector;                 /**< ID sector number */
    uint8_t size_code;              /**< Sector size code */
    uint8_t flags;                  /**< Flags (bit 0 = dup, bit 2 = CRC error, etc.) */
    uint8_t crc;                    /**< CRC-8 */
} uft_td0_sector_header_t;
#pragma pack(pop)

/**
 * @brief TD0 sector data header
 */
#pragma pack(push, 1)
typedef struct uft_td0_data_header {
    uint16_t size;                  /**< Data size (little-endian) */
    uint8_t encoding;               /**< Encoding method */
} uft_td0_data_header_t;
#pragma pack(pop)

/**
 * @brief TD0 sector
 */
typedef struct uft_td0_sector {
    uft_td0_sector_header_t header;
    uint8_t* data;                  /**< Decoded sector data */
    size_t data_size;               /**< Size of decoded data */
    bool has_data;                  /**< Whether sector has data */
    bool crc_error;                 /**< CRC error flag */
    bool deleted;                   /**< Deleted data mark */
} uft_td0_sector_t;

/**
 * @brief TD0 track
 */
typedef struct uft_td0_track {
    int cylinder;
    int head;
    int sector_count;
    uft_td0_sector_t* sectors;      /**< Array of sectors */
} uft_td0_track_t;

/**
 * @brief TD0 image
 */
typedef struct uft_td0_image {
    uft_td0_header_t header;
    uft_td0_comment_t comment;
    bool has_comment;
    
    int track_count;
    uft_td0_track_t* tracks;        /**< Array of tracks */
    
    /* Derived geometry */
    int max_cylinder;
    int max_head;
    int max_sector;
    int sector_size;
} uft_td0_image_t;

/*============================================================================
 * DECOMPRESSION FUNCTIONS
 *============================================================================*/

/**
 * @brief Decompress LZHUF data (TD0 2.x)
 * 
 * Implements the LZSS + Adaptive Huffman algorithm used by TeleDisk 2.x.
 * Based on DiskImageTool's TD0Lzhuf implementation.
 * 
 * @param src Compressed data
 * @param src_len Length of compressed data
 * @param dst Output buffer
 * @param max_out Maximum output size
 * @return Bytes decompressed, or negative on error
 */
int uft_td0_lzhuf_decompress(const uint8_t* src, size_t src_len,
                              uint8_t* dst, size_t max_out);

/**
 * @brief Decompress LZW data (TD0 1.x)
 * 
 * Implements the 12-bit LZW algorithm used by TeleDisk 1.x.
 * Based on DiskImageTool's TD0Lzw implementation.
 * 
 * @param src Compressed data
 * @param src_len Length of compressed data
 * @param dst Output buffer
 * @param max_out Maximum output size
 * @return Bytes decompressed, or negative on error
 */
int uft_td0_lzw_decompress(const uint8_t* src, size_t src_len,
                            uint8_t* dst, size_t max_out);

/**
 * @brief Expand RLE encoded sector data
 */
int uft_td0_rle_expand(const uint8_t* src, size_t src_len,
                        uint8_t* dst, size_t expected_size);

/*============================================================================
 * TD0 CRC FUNCTIONS
 *============================================================================*/

/**
 * @brief Calculate TD0 CRC-16 (header CRC)
 */
uint16_t uft_td0_crc16(const uint8_t* data, size_t len);

/**
 * @brief Calculate TD0 CRC-8 (track/sector CRC)
 */
uint8_t uft_td0_crc8(const uint8_t* data, size_t len);

/*============================================================================
 * TD0 IMAGE OPERATIONS
 *============================================================================*/

/**
 * @brief Open and parse a TD0 image file
 * 
 * @param filename Path to TD0 file
 * @param image Output image structure
 * @return 0 on success, negative on error
 */
int uft_td0_open(const char* filename, uft_td0_image_t* image);

/**
 * @brief Parse TD0 data from memory
 */
int uft_td0_parse(const uint8_t* data, size_t len, uft_td0_image_t* image);

/**
 * @brief Close and free TD0 image resources
 */
void uft_td0_close(uft_td0_image_t* image);

/**
 * @brief Read a sector from TD0 image
 * 
 * @param image TD0 image
 * @param cylinder Cylinder number
 * @param head Head number
 * @param sector Sector number (1-based)
 * @param buffer Output buffer (must be >= sector_size)
 * @return 0 on success, negative on error
 */
int uft_td0_read_sector(const uft_td0_image_t* image,
                        int cylinder, int head, int sector,
                        uint8_t* buffer);

/**
 * @brief Convert TD0 image to raw sector image
 * 
 * @param image TD0 image
 * @param output Output buffer
 * @param max_size Maximum output size
 * @return Bytes written, or negative on error
 */
int uft_td0_to_raw(const uft_td0_image_t* image,
                   uint8_t* output, size_t max_size);

/**
 * @brief Check if TD0 header is valid
 */
bool uft_td0_validate_header(const uft_td0_header_t* header);

/**
 * @brief Check if file is compressed (LZHUF)
 */
static inline bool uft_td0_is_compressed(const uft_td0_header_t* header) {
    return (header->signature[0] == 't' && header->signature[1] == 'd');
}

/**
 * @brief Check if file has comment block
 */
static inline bool uft_td0_has_comment(const uft_td0_header_t* header) {
    return (header->stepping & 0x80) != 0;
}

/**
 * @brief Check if format is FM (single density)
 */
static inline bool uft_td0_is_fm(const uft_td0_header_t* header) {
    return (header->data_rate & 0x80) != 0;
}

/**
 * @brief Get data rate from header
 */
static inline uft_td0_rate_t uft_td0_get_rate(const uft_td0_header_t* header) {
    return (uft_td0_rate_t)(header->data_rate & 0x03);
}

/**
 * @brief Get version string (e.g., "2.1")
 */
void uft_td0_version_string(const uft_td0_header_t* header, 
                            char* buffer, size_t buf_len);

/*============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Get human-readable drive type name
 */
const char* uft_td0_drive_name(uft_td0_drive_t drive);

/**
 * @brief Get human-readable data rate name
 */
const char* uft_td0_rate_name(uft_td0_rate_t rate);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TELEDISK_H */
