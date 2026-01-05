/**
 * @file uft_atx.h
 * @brief ATX (Atari 8-bit Extended) Format with Copy Protection
 * 
 * EXT-009: ATX format support for protected Atari 8-bit disks
 * 
 * ATX is a preservation format that captures:
 * - Timing information for each sector
 * - Weak/fuzzy bit regions
 * - Extended sector data
 * - Copy protection features
 */

#ifndef UFT_ATX_H
#define UFT_ATX_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "uft/uft_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

#define UFT_ATX_MAGIC           0x41543858  /* 'AT8X' */
#define UFT_ATX_VERSION         1
#define UFT_ATX_MAX_TRACKS      40
#define UFT_ATX_MAX_SECTORS     26

/* Sector status flags */
#define UFT_ATX_STATUS_FDC_MASK     0x3F    /**< FDC status bits */
#define UFT_ATX_STATUS_EXTENDED     0x40    /**< Extended data present */
#define UFT_ATX_STATUS_WEAK         0x80    /**< Weak/fuzzy bits */

/* FDC status bits (compatible with Atari FDC) */
#define UFT_ATX_FDC_BUSY            0x01
#define UFT_ATX_FDC_DRQ             0x02
#define UFT_ATX_FDC_LOST_DATA       0x04
#define UFT_ATX_FDC_CRC_ERROR       0x08
#define UFT_ATX_FDC_RNF             0x10    /**< Record Not Found */
#define UFT_ATX_FDC_DELETED         0x20    /**< Deleted Data Mark */
#define UFT_ATX_FDC_WPROT           0x40
#define UFT_ATX_FDC_NOT_READY       0x80

/* Extended data types */
#define UFT_ATX_EXT_WEAK_BITS       1       /**< Weak bit mask */
#define UFT_ATX_EXT_LONG_DATA       2       /**< Extended sector data */
#define UFT_ATX_EXT_PHANTOM         3       /**< Phantom sector */

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/**
 * @brief ATX file header
 */
UFT_PACK_BEGIN
typedef struct {
    uint32_t magic;                 /**< 'AT8X' */
    uint16_t version;               /**< Format version */
    uint16_t min_version;           /**< Minimum reader version */
    uint16_t creator;               /**< Creator ID */
    uint16_t creator_version;       /**< Creator version */
    uint32_t flags;                 /**< File flags */
    uint16_t image_type;            /**< Disk type */
    uint8_t  density;               /**< Density (0=SD, 1=ED, 2=DD) */
    uint8_t  reserved1;
    uint32_t image_id;              /**< Unique image ID */
    uint16_t image_version;         /**< Image version */
    uint16_t reserved2;
    uint32_t start_offset;          /**< Offset to first track */
    uint32_t end_offset;            /**< Offset past last track */
} uft_atx_header_t;
UFT_PACK_END

/**
 * @brief Track header
 */
UFT_PACK_BEGIN
typedef struct {
    uint32_t size;                  /**< Track record size */
    uint16_t type;                  /**< Record type (0 = track) */
    uint16_t reserved;
    uint8_t  track_number;          /**< Physical track number */
    uint8_t  side;                  /**< Side (always 0 for Atari 8-bit) */
    uint16_t sector_count;          /**< Number of sectors */
    uint16_t rate;                  /**< Data rate (0 = default) */
    uint16_t reserved2;
    uint32_t flags;                 /**< Track flags */
    uint32_t header_size;           /**< Size of sector headers */
    uint8_t  reserved3[8];
} uft_atx_track_header_t;
UFT_PACK_END

/**
 * @brief Sector header
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  number;                /**< Sector number */
    uint8_t  status;                /**< FDC status + flags */
    uint16_t position;              /**< Angular position (0-26041) */
    uint32_t start_data;            /**< Offset to sector data */
} uft_atx_sector_header_t;
UFT_PACK_END

/**
 * @brief Extended sector data header
 */
UFT_PACK_BEGIN
typedef struct {
    uint32_t size;                  /**< Total size including header */
    uint8_t  type;                  /**< Extended data type */
    uint8_t  sector_index;          /**< Which sector this applies to */
    uint16_t reserved;
} uft_atx_extended_header_t;
UFT_PACK_END

/**
 * @brief Weak bit region
 */
typedef struct {
    uint16_t offset;                /**< Offset in sector */
    uint16_t length;                /**< Length in bits */
    uint8_t  pattern[128];          /**< Weak bit mask */
} uft_atx_weak_region_t;

/**
 * @brief Sector info
 */
typedef struct {
    uint8_t  number;                /**< Sector number (1-26) */
    uint8_t  status;                /**< FDC status */
    uint16_t position;              /**< Angular position */
    uint16_t size;                  /**< Actual data size */
    
    bool has_crc_error;
    bool is_deleted;
    bool has_weak_bits;
    bool is_missing;
    bool is_phantom;
    
    /* Timing */
    uint32_t timing_us;             /**< Read time in microseconds */
    
    /* Weak bits */
    uft_atx_weak_region_t weak[8];
    uint8_t weak_count;
} uft_atx_sector_info_t;

/**
 * @brief Track info
 */
typedef struct {
    uint8_t  track_number;
    uint16_t sector_count;
    uft_atx_sector_info_t sectors[UFT_ATX_MAX_SECTORS];
    
    /* Protection indicators */
    bool has_timing_protection;
    bool has_phantom_sectors;
    bool has_duplicate_sectors;
    bool has_weak_sectors;
    uint8_t missing_sectors;
} uft_atx_track_info_t;

/**
 * @brief ATX file context
 */
typedef struct {
    uft_atx_header_t header;
    
    const uint8_t *data;
    size_t size;
    
    /* Track info cache */
    uft_atx_track_info_t tracks[UFT_ATX_MAX_TRACKS];
    uint8_t track_count;
    
    /* Statistics */
    uint16_t total_sectors;
    uint16_t error_sectors;
    uint16_t weak_sectors;
    uint16_t phantom_sectors;
} uft_atx_ctx_t;

/**
 * @brief Copy protection detection result
 */
typedef struct {
    bool detected;
    float confidence;
    
    /* Protection types */
    bool timing_based;
    bool weak_bit_based;
    bool phantom_sector;
    bool duplicate_sectors;
    bool missing_sectors;
    
    char protection_name[64];
    
    /* Statistics */
    uint8_t affected_tracks;
    uint16_t timing_variations;
    uint16_t weak_bit_regions;
} uft_atx_protection_result_t;

/*===========================================================================
 * Function Prototypes
 *===========================================================================*/

/**
 * @brief Check if file is ATX format
 */
bool uft_atx_is_atx(const uint8_t *data, size_t size);

/**
 * @brief Open ATX file
 */
int uft_atx_open(uft_atx_ctx_t *ctx, const uint8_t *data, size_t size);

/**
 * @brief Close ATX context
 */
void uft_atx_close(uft_atx_ctx_t *ctx);

/**
 * @brief Get track info
 */
int uft_atx_get_track(uft_atx_ctx_t *ctx, uint8_t track,
                      uft_atx_track_info_t *info);

/**
 * @brief Read sector data
 */
int uft_atx_read_sector(uft_atx_ctx_t *ctx, uint8_t track, uint8_t sector,
                        uint8_t *buffer, size_t max_size, size_t *bytes_read);

/**
 * @brief Read sector with timing
 */
int uft_atx_read_sector_timed(uft_atx_ctx_t *ctx, uint8_t track, uint8_t sector,
                              uint8_t *buffer, size_t max_size,
                              uint32_t *timing_us, uint8_t *fdc_status);

/**
 * @brief Detect copy protection
 */
int uft_atx_detect_protection(uft_atx_ctx_t *ctx,
                              uft_atx_protection_result_t *result);

/**
 * @brief Convert ATX to raw sector image (ATR)
 */
int uft_atx_to_atr(uft_atx_ctx_t *ctx, uint8_t *output, size_t max_size,
                   size_t *output_size);

/**
 * @brief Export protection report
 */
int uft_atx_report_json(const uft_atx_ctx_t *ctx,
                        const uft_atx_protection_result_t *prot,
                        char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ATX_H */
