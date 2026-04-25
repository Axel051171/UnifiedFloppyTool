/**
 * @file uft_imd_adapter.h
 * @brief IMD (ImageDisk) Format Adapter
 * @version 1.0.0
 * 
 * IMD Format: Created by Dave Dunfield for ImageDisk
 * - ASCII header with date/time and optional comments
 * - Track/sector structure with mode bytes
 * - FM and MFM encoding support
 * - Sector compression and error flags
 * 
 * Uses: uft_imd_data_modes.h for mode definitions
 */

#ifndef UFT_IMD_ADAPTER_H
#define UFT_IMD_ADAPTER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "uft/xdf/uft_xdf_adapter.h"
#include "uft/profiles/uft_imd_data_modes.h"
#include "uft/core/uft_error_codes.h"
#include "uft/core/uft_format_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * IMD Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_IMD_SIGNATURE       "IMD "
#define UFT_IMD_SIGNATURE_LEN   4
#define UFT_IMD_HEADER_END      0x1A   /* ASCII EOF terminates header */
#define UFT_IMD_MAX_TRACKS      160    /* Max cylinders × 2 sides */
#define UFT_IMD_MAX_SECTORS     64     /* Max sectors per track */
#define UFT_IMD_MAX_COMMENT     4096   /* Max comment length */

/* Format ID for adapter registration */
#define UFT_FORMAT_ID_IMD       0x494D4400  /* 'IMD\0' */

/* ═══════════════════════════════════════════════════════════════════════════════
 * IMD Track Header
 * ═══════════════════════════════════════════════════════════════════════════════
 * 
 * Structure per track (5 bytes minimum):
 *   [Mode][Cylinder][Head][NumSectors][SectorSize]
 *   optional: [SectorMap][CylinderMap][HeadMap]
 */

/**
 * IMD Track Header Flags (in Head byte)
 */
#define UFT_IMD_HEAD_MASK       0x01   /* Head number (0 or 1) */
#define UFT_IMD_HAS_SECTOR_MAP  0x80   /* Sector numbering map follows */
#define UFT_IMD_HAS_CYLINDER_MAP 0x40  /* Cylinder map follows */
#define UFT_IMD_HAS_HEAD_MAP    0x20   /* Head map follows */

/**
 * IMD Track structure
 */
typedef struct {
    uint8_t mode;               /**< Data mode (FM/MFM + rate) */
    uint8_t cylinder;           /**< Physical cylinder */
    uint8_t head;               /**< Head + flags */
    uint8_t sector_count;       /**< Number of sectors */
    uint8_t sector_size_code;   /**< Sector size code (0-6) */
    
    /* Optional maps (if flags set) */
    uint8_t sector_map[UFT_IMD_MAX_SECTORS];    /**< Sector IDs */
    uint8_t cylinder_map[UFT_IMD_MAX_SECTORS];  /**< Cylinder IDs */
    uint8_t head_map[UFT_IMD_MAX_SECTORS];      /**< Head IDs */
    
    /* Decoded info */
    uint16_t sector_size;       /**< Actual sector size in bytes */
    bool is_fm;                 /**< true if FM encoding */
    uint32_t bitrate;           /**< Bit rate in kbps */
} uft_imd_track_header_t;

/**
 * IMD Sector structure
 */
typedef struct {
    uint8_t id;                 /**< Sector ID */
    uint8_t cylinder_id;        /**< Cylinder ID (from map or track) */
    uint8_t head_id;            /**< Head ID (from map or track) */
    uint8_t status;             /**< Sector status (UFT_IMD_SECTOR_*) */
    
    uint8_t *data;              /**< Sector data (NULL if unavailable) */
    uint16_t data_size;         /**< Data size in bytes */
    
    bool is_compressed;         /**< Data is compressed (single byte) */
    bool is_deleted;            /**< Deleted data address mark */
    bool has_error;             /**< Data CRC error */
} uft_imd_sector_t;

/**
 * IMD File context
 */
typedef struct {
    /* Header */
    char version[32];           /**< IMD version string */
    char date[32];              /**< Creation date */
    char *comment;              /**< Optional comment (malloc'd) */
    size_t comment_len;         /**< Comment length */
    
    /* Tracks */
    uft_imd_track_header_t tracks[UFT_IMD_MAX_TRACKS];
    size_t track_count;
    
    /* Source data */
    const uint8_t *data;
    size_t data_size;
    
    /* Track offsets */
    size_t track_offsets[UFT_IMD_MAX_TRACKS];
    
    /* Statistics */
    uint32_t total_sectors;
    uint32_t error_sectors;
    uint32_t deleted_sectors;
    uint32_t compressed_sectors;
    
    /* Detected geometry */
    uint8_t max_cylinder;
    uint8_t max_head;
    uint8_t max_sectors;
    uint16_t max_sector_size;
} uft_imd_context_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Probe / Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * Probe data for IMD format
 * 
 * @param data Input data
 * @param size Data size
 * @param score Output score
 * @return UFT_OK if format recognized
 */
uft_error_t uft_imd_probe(
    const uint8_t *data,
    size_t size,
    uft_format_score_t *score
);

/**
 * Quick check if data starts with IMD signature
 * 
 * @param data Input data
 * @param size Data size
 * @return true if IMD signature found
 */
static inline bool uft_imd_check_signature(const uint8_t *data, size_t size) {
    if (!data || size < UFT_IMD_SIGNATURE_LEN) return false;
    return data[0] == 'I' && data[1] == 'M' && data[2] == 'D' && data[3] == ' ';
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Open / Close
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * Open IMD file and parse structure
 * 
 * @param data Input data
 * @param size Data size
 * @param ctx Output context (caller-allocated or NULL to malloc)
 * @return UFT_OK on success
 */
uft_error_t uft_imd_open(
    const uint8_t *data,
    size_t size,
    uft_imd_context_t **ctx
);

/**
 * Close IMD context and free resources
 * 
 * @param ctx Context to close
 */
void uft_imd_close(uft_imd_context_t *ctx);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Track / Sector Access
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * Get track data
 * 
 * @param ctx IMD context
 * @param track Physical track number
 * @param side Side (0 or 1)
 * @param out Output track data
 * @return UFT_OK on success
 */
uft_error_t uft_imd_get_track(
    const uft_imd_context_t *ctx,
    uint8_t track,
    uint8_t side,
    uft_track_data_t *out
);

/**
 * Get sector data
 * 
 * @param ctx IMD context
 * @param track Physical track
 * @param side Side
 * @param sector Sector ID
 * @param out Output sector data
 * @return UFT_OK on success
 */
uft_error_t uft_imd_get_sector(
    const uft_imd_context_t *ctx,
    uint8_t track,
    uint8_t side,
    uint8_t sector,
    uft_sector_data_t *out
);

/**
 * Read sector data into buffer
 * 
 * @param ctx IMD context
 * @param track Physical track
 * @param side Side
 * @param sector Sector ID
 * @param buffer Output buffer
 * @param buf_size Buffer size
 * @param bytes_read Output: bytes read
 * @return UFT_OK on success
 */
uft_error_t uft_imd_read_sector(
    const uft_imd_context_t *ctx,
    uint8_t track,
    uint8_t side,
    uint8_t sector,
    uint8_t *buffer,
    size_t buf_size,
    size_t *bytes_read
);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Format Information
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * Get IMD file information
 * 
 * @param ctx IMD context
 * @param info Output format info
 * @return UFT_OK on success
 */
uft_error_t uft_imd_get_info(
    const uft_imd_context_t *ctx,
    uft_format_info_t *info
);

/**
 * Get comment from IMD file
 * 
 * @param ctx IMD context
 * @param buffer Output buffer
 * @param buf_size Buffer size
 * @return Comment length or 0 if none
 */
size_t uft_imd_get_comment(
    const uft_imd_context_t *ctx,
    char *buffer,
    size_t buf_size
);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Adapter Registration
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * Get IMD format adapter for registration with XDF system
 * 
 * @return Pointer to static adapter structure
 */
const uft_format_adapter_t *uft_imd_get_adapter(void);

/**
 * Register IMD adapter with XDF system
 * 
 * @return UFT_OK on success
 */
uft_error_t uft_imd_register(void);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * Parse IMD header (version, date, comment)
 * 
 * @param data Input data
 * @param size Data size
 * @param version Output version string (32 bytes)
 * @param date Output date string (32 bytes)
 * @param comment Output comment (caller frees)
 * @param header_end Output: offset after header
 * @return UFT_OK on success
 */
uft_error_t uft_imd_parse_header(
    const uint8_t *data,
    size_t size,
    char *version,
    char *date,
    char **comment,
    size_t *header_end
);

/**
 * Parse single track header
 * 
 * @param data Data at track start
 * @param size Remaining size
 * @param track Output track header
 * @param bytes_consumed Output: bytes consumed
 * @return UFT_OK on success
 */
uft_error_t uft_imd_parse_track_header(
    const uint8_t *data,
    size_t size,
    uft_imd_track_header_t *track,
    size_t *bytes_consumed
);

/**
 * Get encoding name for mode
 * 
 * @param mode IMD mode byte
 * @return "FM" or "MFM"
 */
static inline const char *uft_imd_encoding_name(uint8_t mode) {
    const uft_imd_data_mode_t *m = uft_imd_lookup_mode(mode);
    return m ? (m->is_fm ? "FM" : "MFM") : "Unknown";
}

/**
 * Get bitrate for mode
 * 
 * @param mode IMD mode byte
 * @return Bitrate in kbps, or 0 if unknown
 */
static inline uint32_t uft_imd_mode_bitrate(uint8_t mode) {
    const uft_imd_data_mode_t *m = uft_imd_lookup_mode(mode);
    return m ? m->bitrate_kbps : 0;
}

/**
 * Calculate total capacity
 * 
 * @param ctx IMD context
 * @return Total capacity in bytes
 */
static inline uint64_t uft_imd_total_capacity(const uft_imd_context_t *ctx) {
    if (!ctx) return 0;
    uint64_t total = 0;
    for (size_t i = 0; i < ctx->track_count; i++) {
        total += ctx->tracks[i].sector_count * ctx->tracks[i].sector_size;
    }
    return total;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_IMD_ADAPTER_H */
