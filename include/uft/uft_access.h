/**
 * @file uft_access.h
 * @brief Extended Access API - Enhanced a8rawconv compatibility
 * 
 * Provides backwards-compatible extensions to the standard CHS access pattern
 * with explicit geometry, format hints, progress callbacks, and verification.
 * 
 * Key improvements over basic a8rawconv:
 * - Explicit geometry (eliminates heuristics)
 * - Format detection override
 * - Sector numbering clarity (0-based vs 1-based)
 * - Progress callbacks for GUI integration
 * - Post-write verification
 * - Detailed error context
 * 
 * @version 2.10.0
 * @date 2024-12-26
 */

#ifndef UFT_ACCESS_H
#define UFT_ACCESS_H

#include "uft/uft_error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Disk geometry specification
 */
typedef struct {
    uint32_t cylinders;      /**< Number of cylinders/tracks */
    uint32_t heads;          /**< Number of heads/sides */
    uint32_t sectors;        /**< Sectors per track */
    uint32_t sector_size;    /**< Bytes per sector */
} uft_geometry_t;

/**
 * @brief Log level for callbacks
 */
typedef enum {
    UFT_LOG_DEBUG = 0,
    UFT_LOG_INFO = 1,
    UFT_LOG_WARN = 2,
    UFT_LOG_ERROR = 3
} uft_log_level_t;

/**
 * @brief Progress callback signature
 * 
 * @param user_data User-provided context
 * @param current Current item (sector, track, etc.)
 * @param total Total items
 */
typedef void (*uft_progress_fn)(void* user_data, uint32_t current, uint32_t total);

/**
 * @brief Log callback signature
 * 
 * @param user_data User-provided context
 * @param level Log level
 * @param message Log message
 */
typedef void (*uft_log_fn)(void* user_data, uft_log_level_t level, const char* message);

/**
 * @brief Access operation flags
 */
typedef enum {
    UFT_FLAG_NONE = 0,
    
    /** Verify data after write operations */
    UFT_FLAG_VERIFY_WRITE = (1 << 0),
    
    /** Automatically fix CRC errors when possible */
    UFT_FLAG_AUTO_FIX_CRC = (1 << 1),
    
    /** Continue on non-fatal errors */
    UFT_FLAG_IGNORE_ERRORS = (1 << 2),
    
    /** Preserve flux-level metadata when available */
    UFT_FLAG_PRESERVE_FLUX = (1 << 3),
    
    /** Force read-only mode even if file is writable */
    UFT_FLAG_READ_ONLY = (1 << 4),
    
    /** Enable detailed logging */
    UFT_FLAG_VERBOSE = (1 << 5)
} uft_access_flags_t;

/**
 * @brief Extended access options
 * 
 * All fields are optional (can be NULL/0 for defaults).
 * Enables explicit control over format access parameters.
 */
typedef struct {
    /**
     * Explicit geometry specification
     * NULL = auto-detect from file size/header
     */
    const uft_geometry_t* geometry;
    
    /**
     * Format hint for detection override
     * Examples: "ATR", "D64", "IMG", "D88"
     * NULL = auto-detect
     */
    const char* format_hint;
    
    /**
     * Sector ID numbering scheme
     * true  = 0-based (most formats)
     * false = 1-based (Atari, some others)
     * -1    = format default
     */
    int sector_id_zero_based;
    
    /**
     * Operation flags (bitwise OR of uft_access_flags_t)
     */
    uint32_t flags;
    
    /**
     * Progress callback (optional)
     * Called during long operations
     */
    uft_progress_fn progress_callback;
    
    /**
     * Log callback (optional)
     * Called for diagnostic messages
     */
    uft_log_fn log_callback;
    
    /**
     * User data passed to callbacks
     */
    void* user_data;
    
} uft_access_options_t;

/**
 * @brief Initialize options with defaults
 * 
 * @param[out] opts Options structure to initialize
 */
void uft_access_options_init(uft_access_options_t* opts);

/**
 * @brief Read sector with extended options (a8rawconv compatible)
 * 
 * Enhanced version of standard read_sector with explicit control.
 * Fully backwards compatible when opts == NULL.
 * 
 * @param[in] ctx Format context (any uft_*_ctx_t)
 * @param cylinder Cylinder/track number
 * @param head Head/side number
 * @param sector Sector ID (numbering scheme per opts->sector_id_zero_based)
 * @param[out] buffer Buffer to receive data
 * @param buffer_size Buffer size in bytes
 * @param[out] bytes_read Actual bytes read (can be NULL)
 * @param[in] opts Extended options (NULL = defaults)
 * @return UFT_SUCCESS or error code
 * 
 * @note When opts == NULL, behaves exactly like basic read_sector
 */
uft_rc_t uft_read_sector_ex(
    void* ctx,
    uint32_t cylinder,
    uint32_t head,
    uint32_t sector,
    uint8_t* buffer,
    size_t buffer_size,
    size_t* bytes_read,
    const uft_access_options_t* opts
);

/**
 * @brief Write sector with extended options
 * 
 * Enhanced version with verification and progress callbacks.
 * 
 * @param[in] ctx Format context
 * @param cylinder Cylinder number
 * @param head Head number
 * @param sector Sector ID
 * @param[in] data Data to write
 * @param data_size Data size in bytes
 * @param[in] opts Extended options (NULL = defaults)
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_write_sector_ex(
    void* ctx,
    uint32_t cylinder,
    uint32_t head,
    uint32_t sector,
    const uint8_t* data,
    size_t data_size,
    const uft_access_options_t* opts
);

/**
 * @brief Bulk read multiple sectors with progress
 * 
 * Reads sequential sectors with progress reporting.
 * Useful for imaging entire tracks/disks.
 * 
 * @param[in] ctx Format context
 * @param start_cylinder Start cylinder
 * @param start_head Start head
 * @param start_sector Start sector
 * @param sector_count Number of sectors to read
 * @param[out] buffer Buffer for all sectors (must be large enough)
 * @param buffer_size Total buffer size
 * @param[in] opts Extended options (NULL = defaults)
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_read_sectors_bulk(
    void* ctx,
    uint32_t start_cylinder,
    uint32_t start_head,
    uint32_t start_sector,
    uint32_t sector_count,
    uint8_t* buffer,
    size_t buffer_size,
    const uft_access_options_t* opts
);

/**
 * @brief Bulk write multiple sectors with verification
 * 
 * @param[in] ctx Format context
 * @param start_cylinder Start cylinder
 * @param start_head Start head
 * @param start_sector Start sector
 * @param sector_count Number of sectors to write
 * @param[in] data Source data for all sectors
 * @param data_size Total data size
 * @param[in] opts Extended options (NULL = defaults)
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_write_sectors_bulk(
    void* ctx,
    uint32_t start_cylinder,
    uint32_t start_head,
    uint32_t start_sector,
    uint32_t sector_count,
    const uint8_t* data,
    size_t data_size,
    const uft_access_options_t* opts
);

/**
 * @brief Helper: Parse geometry string
 * 
 * Parses strings like "40,1,18,256" or "40x1x18:256"
 * 
 * @param str Geometry string
 * @param[out] geom Parsed geometry
 * @return UFT_SUCCESS or UFT_ERR_INVALID_ARG
 */
uft_rc_t uft_parse_geometry(const char* str, uft_geometry_t* geom);

/**
 * @brief Helper: Geometry to string
 * 
 * Formats geometry as "CxHxS:SIZE"
 * 
 * @param[in] geom Geometry
 * @param[out] buffer Output buffer
 * @param buffer_size Buffer size
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_geometry_to_string(
    const uft_geometry_t* geom,
    char* buffer,
    size_t buffer_size
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ACCESS_H */
