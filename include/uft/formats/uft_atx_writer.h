/**
 * @file uft_atx_writer.h
 * @brief ATX Format Write Support
 * 
 * P2-005: Complete ATX write implementation
 * 
 * Features:
 * - Create ATX from scratch
 * - Convert ATR to ATX with timing
 * - Preserve copy protection
 * - Weak bit encoding
 * - Phantom sector support
 */

#ifndef UFT_ATX_WRITER_H
#define UFT_ATX_WRITER_H

#include "uft/formats/uft_atx.h"
#include "uft/core/uft_unified_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ATX Creator IDs */
#define ATX_CREATOR_UFT         0x5546  /* "UF" */
#define ATX_CREATOR_VERSION     0x0100  /* 1.0 */

/* Image types */
#define ATX_IMAGE_TYPE_NORMAL   0x0001
#define ATX_IMAGE_TYPE_BOOT     0x0002

/* Track flags */
#define ATX_TRACK_HAS_GAPS      0x0001
#define ATX_TRACK_HAS_LONG      0x0002
#define ATX_TRACK_HAS_WEAK      0x0004
#define ATX_TRACK_HAS_PHANTOM   0x0008

/**
 * @brief ATX write options
 */
typedef struct {
    /* Format options */
    uint8_t density;            /* 0=SD, 1=ED, 2=DD */
    bool preserve_timing;       /* Include timing info */
    bool preserve_weak_bits;    /* Include weak bit data */
    bool preserve_errors;       /* Include error flags */
    
    /* Metadata */
    const char *title;          /* Optional title */
    uint32_t image_id;          /* 0 = auto-generate */
    
    /* Timing defaults */
    uint16_t default_sector_time_us;    /* Default: ~1040 for SD */
    uint16_t rpm;                        /* Default: 288 */
    
} atx_write_options_t;

/**
 * @brief Sector to write
 */
typedef struct {
    uint8_t number;             /* Sector number (1-26) */
    uint8_t status;             /* FDC status */
    uint16_t position;          /* Angular position (0-26041) */
    
    const uint8_t *data;        /* Sector data */
    uint16_t data_size;         /* Usually 128 or 256 */
    
    /* Timing */
    uint32_t timing_us;         /* Read time (0 = default) */
    
    /* Weak bits (optional) */
    const uint8_t *weak_mask;   /* Weak bit mask */
    uint16_t weak_offset;       /* Offset in sector */
    uint16_t weak_length;       /* Length in bytes */
    
    /* Extended data (optional) */
    const uint8_t *extended_data;
    uint16_t extended_size;
    uint8_t extended_type;
    
} atx_write_sector_t;

/**
 * @brief Track to write
 */
typedef struct {
    uint8_t track_number;
    uint8_t side;               /* Always 0 for Atari 8-bit */
    uint32_t flags;             /* Track flags */
    
    atx_write_sector_t *sectors;
    uint16_t sector_count;
    uint16_t sector_capacity;
    
} atx_write_track_t;

/**
 * @brief ATX writer context
 */
typedef struct {
    atx_write_options_t opts;
    
    atx_write_track_t tracks[UFT_ATX_MAX_TRACKS];
    uint8_t track_count;
    
    /* Statistics */
    uint16_t total_sectors;
    uint16_t error_sectors;
    uint16_t weak_sectors;
    size_t data_size;
    
} atx_writer_ctx_t;

/* ============================================================================
 * Initialization
 * ============================================================================ */

/**
 * @brief Initialize write options
 */
void atx_write_options_init(atx_write_options_t *opts);

/**
 * @brief Initialize writer context
 */
void atx_writer_init(atx_writer_ctx_t *ctx);

/**
 * @brief Free writer context
 */
void atx_writer_free(atx_writer_ctx_t *ctx);

/* ============================================================================
 * Track/Sector Building
 * ============================================================================ */

/**
 * @brief Add track to writer
 */
int atx_writer_add_track(atx_writer_ctx_t *ctx, uint8_t track_num);

/**
 * @brief Add sector to track
 */
int atx_writer_add_sector(atx_writer_ctx_t *ctx, uint8_t track_num,
                          const atx_write_sector_t *sector);

/**
 * @brief Add simple sector (no extended data)
 */
int atx_writer_add_sector_simple(atx_writer_ctx_t *ctx, uint8_t track_num,
                                 uint8_t sector_num, const uint8_t *data,
                                 uint16_t data_size, uint8_t status);

/**
 * @brief Add sector with timing
 */
int atx_writer_add_sector_timed(atx_writer_ctx_t *ctx, uint8_t track_num,
                                uint8_t sector_num, uint16_t position,
                                const uint8_t *data, uint16_t data_size,
                                uint32_t timing_us, uint8_t status);

/**
 * @brief Add weak bit sector
 */
int atx_writer_add_sector_weak(atx_writer_ctx_t *ctx, uint8_t track_num,
                               uint8_t sector_num, const uint8_t *data,
                               uint16_t data_size, const uint8_t *weak_mask,
                               uint16_t weak_offset, uint16_t weak_length);

/**
 * @brief Add phantom sector
 */
int atx_writer_add_phantom(atx_writer_ctx_t *ctx, uint8_t track_num,
                           uint8_t sector_num, uint16_t position);

/**
 * @brief Set sector angular positions automatically
 */
int atx_writer_auto_positions(atx_writer_ctx_t *ctx, uint8_t track_num);

/* ============================================================================
 * Writing
 * ============================================================================ */

/**
 * @brief Calculate required buffer size
 */
size_t atx_writer_calculate_size(const atx_writer_ctx_t *ctx);

/**
 * @brief Write ATX to buffer
 */
uft_error_t atx_writer_write(const atx_writer_ctx_t *ctx,
                             uint8_t *buffer, size_t buffer_size,
                             size_t *out_size);

/**
 * @brief Write ATX to file
 */
uft_error_t atx_writer_write_file(const atx_writer_ctx_t *ctx,
                                  const char *path);

/* ============================================================================
 * Conversion
 * ============================================================================ */

/**
 * @brief Convert ATR to ATX
 */
uft_error_t uft_atr_to_atx(const uint8_t *atr_data, size_t atr_size,
                           uint8_t *atx_buffer, size_t buffer_size,
                           size_t *out_size,
                           const atx_write_options_t *opts);

/**
 * @brief Convert XFD to ATX
 */
uft_error_t uft_xfd_to_atx(const uint8_t *xfd_data, size_t xfd_size,
                           uint8_t *atx_buffer, size_t buffer_size,
                           size_t *out_size,
                           const atx_write_options_t *opts);

/**
 * @brief Convert disk image to ATX
 */
uft_error_t uft_disk_to_atx(const uft_disk_image_t *disk,
                            uint8_t *atx_buffer, size_t buffer_size,
                            size_t *out_size,
                            const atx_write_options_t *opts);

/**
 * @brief Write disk image as ATX file
 */
uft_error_t uft_atx_write(const uft_disk_image_t *disk,
                          const char *path,
                          const atx_write_options_t *opts);

/* ============================================================================
 * Timing Helpers
 * ============================================================================ */

/**
 * @brief Calculate angular position from sector number
 * @param sector Sector number (1-18/26)
 * @param sectors_per_track Total sectors
 * @return Position (0-26041)
 */
uint16_t atx_sector_position(uint8_t sector, uint8_t sectors_per_track);

/**
 * @brief Calculate default timing for sector
 * @param density 0=SD, 1=ED, 2=DD
 * @return Time in microseconds
 */
uint32_t atx_default_timing(uint8_t density);

/**
 * @brief Generate timing variation for protection
 */
uint32_t atx_timing_variation(uint32_t base_timing, int variation_percent);

/* ============================================================================
 * Weak Bit Helpers
 * ============================================================================ */

/**
 * @brief Create weak bit mask for entire sector
 */
void atx_make_weak_mask_full(uint8_t *mask, uint16_t sector_size);

/**
 * @brief Create weak bit mask for region
 */
void atx_make_weak_mask_region(uint8_t *mask, uint16_t sector_size,
                               uint16_t start, uint16_t length);

/**
 * @brief Generate random weak bit data
 */
void atx_apply_weak_bits(uint8_t *data, const uint8_t *mask, uint16_t size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ATX_WRITER_H */
