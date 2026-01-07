/**
 * @file uft_td0_writer.h
 * @brief Teledisk (TD0) format writer
 * 
 * P0-003: TD0 Writer Implementation
 * 
 * TD0 is a critical format - without a writer, data can be LOST
 * when users need to convert to/from TD0.
 * 
 * TD0 Format Specification:
 * - Header: 12 bytes (signature, version, etc.)
 * - Optional comment block
 * - Track/sector data with optional LZSS compression
 * - CRC on each sector
 */

#ifndef UFT_TD0_WRITER_H
#define UFT_TD0_WRITER_H

#include "uft/core/uft_unified_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* TD0 Signatures */
#define TD0_SIG_NORMAL      "TD"   /* Normal (uncompressed) */
#define TD0_SIG_ADVANCED    "td"   /* Advanced (LZSS compressed) */

/* TD0 Versions */
#define TD0_VERSION_10      0x10
#define TD0_VERSION_11      0x11
#define TD0_VERSION_15      0x15
#define TD0_VERSION_20      0x20
#define TD0_VERSION_21      0x21

/* TD0 Density */
#define TD0_DENSITY_250K    0x00   /* 250 Kbps (DD) */
#define TD0_DENSITY_300K    0x01   /* 300 Kbps */
#define TD0_DENSITY_500K    0x02   /* 500 Kbps (HD) */

/* TD0 Drive Type */
#define TD0_DRIVE_525_360   0x01   /* 5.25" 360K */
#define TD0_DRIVE_525_12    0x02   /* 5.25" 1.2M */
#define TD0_DRIVE_35_720    0x03   /* 3.5" 720K */
#define TD0_DRIVE_35_144    0x04   /* 3.5" 1.44M */
#define TD0_DRIVE_8_SD      0x05   /* 8" SD */
#define TD0_DRIVE_8_DD      0x06   /* 8" DD */

/* Sector flags */
#define TD0_SECT_NORMAL     0x00
#define TD0_SECT_DUPLICATE  0x01
#define TD0_SECT_CRC_ERROR  0x02
#define TD0_SECT_DELETED    0x04
#define TD0_SECT_SKIPPED    0x10
#define TD0_SECT_NO_DAM     0x20
#define TD0_SECT_NO_DATA    0x30

/**
 * @brief TD0 write options
 */
typedef struct {
    /* Compression */
    bool use_advanced_compression;  /**< Use LZSS compression */
    uint8_t compression_level;      /**< 0-9 (higher = slower but smaller) */
    
    /* Comment */
    bool include_comment;           /**< Include comment block */
    const char *comment;            /**< Comment text (NULL = auto-generate) */
    
    /* Date */
    bool include_date;              /**< Include creation date */
    uint16_t year;                  /**< Year (0 = current) */
    uint8_t month;                  /**< Month (0 = current) */
    uint8_t day;                    /**< Day (0 = current) */
    
    /* Format hints */
    uint8_t drive_type;             /**< TD0_DRIVE_* or 0 for auto */
    uint8_t density;                /**< TD0_DENSITY_* or 0 for auto */
    uint8_t stepping;               /**< 0=single, 1=double, 2=even-only */
    bool dos_alloc;                 /**< DOS allocation mode */
    
    /* Error handling */
    bool preserve_errors;           /**< Preserve error flags */
    bool preserve_deleted;          /**< Preserve deleted marks */
    
} uft_td0_write_options_t;

/**
 * @brief TD0 write result/statistics
 */
typedef struct {
    size_t tracks_written;
    size_t sectors_written;
    size_t bytes_written;
    size_t bytes_compressed;        /**< After compression */
    double compression_ratio;
    
    size_t error_sectors;           /**< Sectors with errors preserved */
    size_t deleted_sectors;         /**< Deleted sectors preserved */
    
    bool success;
    uft_error_t error;
    const char *error_detail;
    
} uft_td0_write_result_t;

/**
 * @brief Initialize TD0 write options with defaults
 */
void uft_td0_write_options_init(uft_td0_write_options_t *opts);

/**
 * @brief Write disk image to TD0 file
 * @param image Source disk image
 * @param path Output file path
 * @param opts Write options (NULL for defaults)
 * @return UFT_OK on success
 */
uft_error_t uft_td0_write(const uft_disk_image_t *image,
                          const char *path,
                          const uft_td0_write_options_t *opts);

/**
 * @brief Write disk image to TD0 with detailed result
 * @param image Source disk image
 * @param path Output file path
 * @param opts Write options (NULL for defaults)
 * @param result Output: detailed results
 * @return UFT_OK on success
 */
uft_error_t uft_td0_write_ex(const uft_disk_image_t *image,
                             const char *path,
                             const uft_td0_write_options_t *opts,
                             uft_td0_write_result_t *result);

/**
 * @brief Write disk image to memory buffer
 * @param image Source disk image
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @param out_size Output: actual size written
 * @param opts Write options (NULL for defaults)
 * @return UFT_OK on success
 */
uft_error_t uft_td0_write_mem(const uft_disk_image_t *image,
                              uint8_t *buffer,
                              size_t buffer_size,
                              size_t *out_size,
                              const uft_td0_write_options_t *opts);

/**
 * @brief Estimate TD0 output size
 * @param image Source disk image
 * @param opts Write options (NULL for defaults)
 * @return Estimated size in bytes
 */
size_t uft_td0_estimate_size(const uft_disk_image_t *image,
                             const uft_td0_write_options_t *opts);

/**
 * @brief Validate disk image for TD0 compatibility
 * @param image Source disk image
 * @param warnings Output: warning messages (optional)
 * @param max_warnings Maximum warnings to collect
 * @return Number of compatibility issues found
 */
int uft_td0_validate(const uft_disk_image_t *image,
                     char **warnings,
                     size_t max_warnings);

/**
 * @brief Auto-detect optimal TD0 settings
 */
void uft_td0_auto_settings(const uft_disk_image_t *image,
                           uft_td0_write_options_t *opts);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TD0_WRITER_H */
