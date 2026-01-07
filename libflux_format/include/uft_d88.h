/**
 * @file uft_d88.h
 * @brief D88/D77/D68 Format - Japanese Multi-Platform Container
 * 
 * D88 is the universal format for Japanese systems:
 * - NEC PC-88, PC-98 series
 * - Sharp X1, X68000
 * - Fujitsu FM-7, FM-77, FM Towns
 * 
 * Format Details:
 * - Track+sector container (not raw CHS dump)
 * - Sectors stored in read order with headers
 * - Per-sector FDC status (CRC errors, deleted data, etc.)
 * - Supports 672-byte (old) or 688-byte (new) headers
 * - Can contain multiple disks (we read first only)
 * 
 * Migrated to UFT v2.10.0:
 * - Unified uft_rc_t error codes
 * - Standard _create()/_destroy() lifecycle
 * - Flux metadata integration
 * - Atari compatible sector access
 * 
 * @version 2.10.0
 * @date 2024-12-26
 * @note Original implementation by UFT contributor 2025
 */

#ifndef UFT_D88_H
#define UFT_D88_H

#include "uft/uft_error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Flux timing profile (for preservation)
 * 
 * D88 stores logical data but we preserve timing hints
 * for future flux reconstruction.
 */
typedef struct {
    /** Nominal bitcell time in nanoseconds */
    uint32_t nominal_cell_ns;
    
    /** Jitter tolerance in nanoseconds */
    uint32_t jitter_ns;
    
    /** Encoding: 0=unknown, 1=MFM, 2=FM, 3=GCR */
    uint32_t encoding_hint;
} uft_d88_flux_timing_t;

/**
 * @brief Weak bit region metadata
 * 
 * D88 doesn't directly represent weak bits, but we
 * store regions for higher-level analysis.
 */
typedef struct {
    uint32_t track;              /**< Physical track */
    uint32_t head;               /**< Head/side */
    uint32_t start_bitcell;      /**< Start position in bitcells */
    uint32_t length_bitcell;     /**< Length in bitcells */
    uint32_t prng_seed;          /**< Seed for weak bit emulation */
} uft_d88_weak_region_t;

/**
 * @brief Flux metadata container
 */
typedef struct {
    uft_d88_flux_timing_t timing;
    uft_d88_weak_region_t* weak_regions;
    uint32_t weak_region_count;
} uft_d88_flux_meta_t;

/**
 * @brief D88 sector information
 * 
 * Exposes per-sector metadata for analysis and protection detection.
 */
typedef struct {
    uint8_t c;                   /**< Cylinder (track) ID */
    uint8_t h;                   /**< Head ID */
    uint8_t r;                   /**< Record (sector) ID */
    uint8_t n;                   /**< Sector size code (n → 128<<n bytes) */
    uint16_t sectors_in_track;   /**< Total sectors on this track */
    uint8_t density_flag;        /**< 0x00=DD, 0x40=SD */
    uint8_t deleted_flag;        /**< 0x00=normal, 0x10=deleted (DDAM) */
    uint8_t status;              /**< FDC status: 0x00=OK, 0xB0=CRC error, etc. */
    uint16_t data_size;          /**< Actual data bytes (may differ from n) */
    uint64_t data_offset;        /**< File offset to sector data */
} uft_d88_sector_info_t;

/**
 * @brief D88 context structure
 */
typedef struct {
    /** File path (owned) */
    char* path;
    
    /** Read-only mode */
    bool read_only;
    
    /** File size */
    uint64_t file_size;
    
    /** Header size: 672 (old) or 688 (new) */
    uint32_t header_size;
    
    /** Disk size from header */
    uint32_t disk_size;
    
    /** Maximum tracks (inferred) */
    uint32_t track_count_max;
    
    /** Track offset table [164] */
    uint32_t track_offsets[164];
    
    /** Sector metadata array */
    uft_d88_sector_info_t* sectors;
    
    /** Total sectors */
    uint32_t sector_count;
    
    /** Geometry (computed) */
    uint32_t tracks;
    uint32_t heads;
    uint32_t sectors_per_track;  /**< Average, may vary */
    uint32_t sector_size;        /**< Typical size */
    
    /** Flux metadata */
    uft_d88_flux_meta_t flux;
    
    /** File handle (internal) */
    void* fp_internal;
    
    /** Error context */
    uft_error_ctx_t error;
} uft_d88_ctx_t;

/**
 * @brief Create D88 context
 * 
 * @param[out] ctx Pointer to receive created context
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_d88_create(uft_d88_ctx_t** ctx);

/**
 * @brief Destroy D88 context
 * 
 * Closes file and frees all resources.
 * Safe to call with NULL or *ctx == NULL.
 * 
 * @param[in,out] ctx Pointer to context (set to NULL after)
 */
void uft_d88_destroy(uft_d88_ctx_t** ctx);

/**
 * @brief Detect if buffer contains D88 format
 * 
 * Checks header signature and validates structure.
 * 
 * @param[in] buffer Data to check
 * @param size Buffer size
 * @param[out] header_size If detected, set to 672 or 688 (can be NULL)
 * @return UFT_SUCCESS if D88 detected, UFT_ERR_FORMAT if not
 */
uft_rc_t uft_d88_detect(const uint8_t* buffer, size_t size, uint32_t* header_size);

/**
 * @brief Open D88 file
 * 
 * @param[in] ctx D88 context (must be created first)
 * @param[in] path File path
 * @param read_only Open in read-only mode
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_d88_open(uft_d88_ctx_t* ctx, const char* path, bool read_only);

/**
 * @brief Read sector by CHS (Atari compatible)
 * 
 * @param[in] ctx D88 context
 * @param track Track number (0-based)
 * @param head Head number (0-based)
 * @param sector Sector number (varies by track)
 * @param[out] buffer Buffer to receive data
 * @param buffer_size Buffer size (must be >= sector size)
 * @param[out] bytes_read Actual bytes read (can be NULL)
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_d88_read_sector(
    uft_d88_ctx_t* ctx,
    uint32_t track,
    uint32_t head,
    uint32_t sector,
    uint8_t* buffer,
    size_t buffer_size,
    size_t* bytes_read
);

/**
 * @brief Write sector by CHS
 * 
 * @param[in] ctx D88 context
 * @param track Track number
 * @param head Head number
 * @param sector Sector number
 * @param[in] data Data to write
 * @param data_size Data size
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_d88_write_sector(
    uft_d88_ctx_t* ctx,
    uint32_t track,
    uint32_t head,
    uint32_t sector,
    const uint8_t* data,
    size_t data_size
);

/**
 * @brief Get sector metadata
 * 
 * Retrieves detailed sector information including FDC status.
 * Useful for copy-protection analysis.
 * 
 * @param[in] ctx D88 context
 * @param track Track number
 * @param head Head number
 * @param sector Sector number
 * @param[out] info Sector info structure
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_d88_get_sector_info(
    uft_d88_ctx_t* ctx,
    uint32_t track,
    uint32_t head,
    uint32_t sector,
    uft_d88_sector_info_t* info
);

/**
 * @brief Export to raw CHS-ordered IMG
 * 
 * Converts D88 to standard IMG format with uniform geometry.
 * 
 * @param[in] ctx D88 context
 * @param[in] output_path Output IMG file path
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_d88_export_img(uft_d88_ctx_t* ctx, const char* output_path);

/**
 * @brief Import raw IMG to D88
 * 
 * Creates D88 from raw IMG with specified geometry.
 * 
 * @param[in] input_path Input IMG file
 * @param[in] output_path Output D88 file
 * @param tracks Number of tracks
 * @param heads Number of heads
 * @param sectors_per_track Sectors per track
 * @param sector_size Sector size in bytes
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_d88_import_img(
    const char* input_path,
    const char* output_path,
    uint32_t tracks,
    uint32_t heads,
    uint32_t sectors_per_track,
    uint32_t sector_size
);

/**
 * @brief Close D88 file
 * 
 * Flushes changes and closes file handle.
 * Context remains valid, can open another file.
 * 
 * @param[in] ctx D88 context
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_d88_close(uft_d88_ctx_t* ctx);

/**
 * @brief Analyze protection schemes
 * 
 * Scans for copy-protection indicators:
 * - CRC errors
 * - Deleted sectors
 * - Non-standard sector sizes
 * - Track layout anomalies
 * 
 * @param[in] ctx D88 context
 * @param[out] protection_found Set to true if protection detected
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_d88_analyze_protection(uft_d88_ctx_t* ctx, bool* protection_found);

#ifdef __cplusplus
}
#endif

#endif /* UFT_D88_H */
