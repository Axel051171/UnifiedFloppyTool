/**
 * @file uft_xdf_adapter.h
 * @brief XDF Format Adapter Interface
 * @version 1.0.0
 * 
 * COUPLING FIX: Establishes clean boundary between:
 * - Format Parsers (src/formats/*)
 * - XDF Core API (src/xdf/*)
 * - Tools (xcopy, recovery, nibble)
 * 
 * Format parsers implement this interface instead of
 * duplicating logic or accessing internals directly.
 * 
 * Plugin-Style Architecture:
 *   1. Format parser provides uft_format_adapter_t
 *   2. XDF API uses adapter for all format-specific operations
 *   3. Tools access data only through XDF API
 */

#ifndef UFT_XDF_ADAPTER_H
#define UFT_XDF_ADAPTER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "uft/core/uft_score.h"
#include "uft/core/uft_error_codes.h"

/* Error type alias */
typedef int uft_error_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Forward Declarations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief XDF context structure
 * 
 * Minimal context for format adapters.
 * Each adapter stores format-specific data in format_data.
 */
struct uft_xdf_context {
    void *format_data;              /**< Format-specific context (adapter-managed) */
    const uint8_t *source_data;     /**< Source data pointer */
    size_t source_size;             /**< Source data size */
    uint32_t format_id;             /**< Detected format ID */
    uint16_t confidence;            /**< Detection confidence */
};

struct uft_track_data;      /* Track data container */
struct uft_sector_data;     /* Sector data container */

/* ═══════════════════════════════════════════════════════════════════════════════
 * Track Data Container
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Universal track data container
 * 
 * All format parsers produce this structure.
 * Tools consume this structure.
 * No format-specific types leak outside.
 */
typedef struct uft_track_data {
    /* Identity */
    uint16_t track_num;             /**< Physical track number */
    uint8_t side;                   /**< Side (0 or 1) */
    uint8_t encoding;               /**< XDF_ENCODING_* */
    
    /* Raw data */
    uint8_t *raw_data;              /**< Raw track data */
    size_t raw_size;                /**< Raw data size */
    
    /* Decoded sectors */
    struct uft_sector_data *sectors;/**< Decoded sectors (array) */
    size_t sector_count;            /**< Number of sectors */
    
    /* Timing (optional) */
    uint32_t *bit_times;            /**< Bit timing array (NULL if unavailable) */
    size_t bit_count;               /**< Number of bits */
    float rpm_measured;             /**< Measured RPM (0 if unknown) */
    
    /* Quality metrics */
    uint16_t confidence;            /**< Overall track confidence (0-10000) */
    uint32_t crc_errors;            /**< CRC error count */
    uint32_t weak_bits;             /**< Weak bit count */
    bool has_protection;            /**< Copy protection detected */
    
    /* Diagnostics */
    char diag_message[128];         /**< Human-readable diagnostic */
} uft_track_data_t;

/**
 * @brief Universal sector data container
 */
typedef struct uft_sector_data {
    /* Identity */
    uint8_t logical_track;          /**< CHRN: C */
    uint8_t head;                   /**< CHRN: H */
    uint8_t sector_id;              /**< CHRN: R */
    uint8_t size_code;              /**< CHRN: N (128 << N) */
    
    /* Data */
    uint8_t *data;                  /**< Sector data */
    size_t data_size;               /**< Actual size */
    
    /* Position */
    uint32_t offset_bits;           /**< Offset from track start (bits) */
    uint32_t offset_us;             /**< Offset from index (microseconds) */
    
    /* Quality */
    uint16_t confidence;            /**< Sector confidence (0-10000) */
    uint8_t status;                 /**< XDF_STATUS_* */
    bool crc_ok;                    /**< CRC valid */
    bool deleted;                   /**< Deleted data mark */
    
    /* FDC status (if available) */
    uint8_t st1;                    /**< FDC ST1 */
    uint8_t st2;                    /**< FDC ST2 */
} uft_sector_data_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Format Adapter Interface
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Format adapter function table
 * 
 * Each format parser provides an instance of this structure.
 * XDF API uses it for all format-specific operations.
 */
typedef struct uft_format_adapter {
    /* Identification */
    const char *name;               /**< Format name (e.g., "ADF", "D64") */
    const char *description;        /**< Human-readable description */
    const char *extensions;         /**< Comma-separated extensions */
    uint32_t format_id;             /**< UFT_FORMAT_ID_* */
    
    /* Capabilities */
    bool can_read;                  /**< Supports reading */
    bool can_write;                 /**< Supports writing */
    bool can_create;                /**< Supports creating new images */
    bool supports_errors;           /**< Stores error information */
    bool supports_timing;           /**< Stores timing information */
    
    /* Detection */
    /**
     * @brief Probe if data matches this format
     * @param data File data
     * @param size Data size
     * @param filename Optional filename for extension check
     * @return Score with confidence
     */
    uft_format_score_t (*probe)(
        const uint8_t *data,
        size_t size,
        const char *filename
    );
    
    /* Reading */
    /**
     * @brief Open and initialize context
     * @param ctx XDF context to initialize
     * @param data Source data
     * @param size Data size
     * @return UFT_SUCCESS or error code
     */
    uft_error_t (*open)(
        struct uft_xdf_context *ctx,
        const uint8_t *data,
        size_t size
    );
    
    /**
     * @brief Read a track
     * @param ctx XDF context
     * @param track Track number
     * @param side Side (0 or 1)
     * @param out Output track data
     * @return UFT_SUCCESS or error code
     */
    uft_error_t (*read_track)(
        struct uft_xdf_context *ctx,
        uint16_t track,
        uint8_t side,
        uft_track_data_t *out
    );
    
    /**
     * @brief Get geometry information
     * @param ctx XDF context
     * @param tracks Output: track count
     * @param sides Output: side count
     * @param sectors Output: sectors per track (max)
     * @param sector_size Output: sector size (bytes)
     */
    void (*get_geometry)(
        struct uft_xdf_context *ctx,
        uint16_t *tracks,
        uint8_t *sides,
        uint8_t *sectors,
        uint16_t *sector_size
    );
    
    /* Writing (optional) */
    /**
     * @brief Write a track
     * @param ctx XDF context
     * @param track Track data to write
     * @return UFT_SUCCESS or error code
     */
    uft_error_t (*write_track)(
        struct uft_xdf_context *ctx,
        const uft_track_data_t *track
    );
    
    /**
     * @brief Export to native format
     * @param ctx XDF context
     * @param output Output buffer (caller allocated)
     * @param size Buffer size
     * @param written Actual bytes written
     * @return UFT_SUCCESS or error code
     */
    uft_error_t (*export_native)(
        struct uft_xdf_context *ctx,
        uint8_t *output,
        size_t size,
        size_t *written
    );
    
    /* Cleanup */
    /**
     * @brief Release resources
     * @param ctx XDF context
     */
    void (*close)(struct uft_xdf_context *ctx);
    
    /* Extension point */
    void *private_data;             /**< Format-specific data */
} uft_format_adapter_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Adapter Registration
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Register a format adapter
 * @param adapter Adapter to register
 * @return UFT_SUCCESS or error code
 */
uft_error_t uft_adapter_register(const uft_format_adapter_t *adapter);

/**
 * @brief Find adapter by format ID
 * @param format_id Format ID
 * @return Adapter or NULL
 */
const uft_format_adapter_t *uft_adapter_find_by_id(uint32_t format_id);

/**
 * @brief Find adapter by extension
 * @param extension File extension (without dot)
 * @return Adapter or NULL
 */
const uft_format_adapter_t *uft_adapter_find_by_extension(const char *extension);

/**
 * @brief Probe all registered adapters
 * @param data File data
 * @param size Data size
 * @param filename Optional filename
 * @param scores Output array of scores
 * @param max_scores Array size
 * @return Number of formats with score > 0
 */
size_t uft_adapter_probe_all(
    const uint8_t *data,
    size_t size,
    const char *filename,
    uft_format_score_t *scores,
    size_t max_scores
);

/**
 * @brief Get best matching adapter
 * @param data File data
 * @param size Data size
 * @param filename Optional filename
 * @param score Output: best score (optional)
 * @return Best adapter or NULL
 */
const uft_format_adapter_t *uft_adapter_detect(
    const uint8_t *data,
    size_t size,
    const char *filename,
    uft_format_score_t *score
);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Track/Sector Helpers
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize track data structure
 */
void uft_track_data_init(uft_track_data_t *track);

/**
 * @brief Free track data structure
 */
void uft_track_data_free(uft_track_data_t *track);

/**
 * @brief Initialize sector data structure
 */
void uft_sector_data_init(uft_sector_data_t *sector);

/**
 * @brief Allocate sectors array for track
 */
uft_error_t uft_track_alloc_sectors(uft_track_data_t *track, size_t count);

/**
 * @brief Find sector in track by ID
 */
uft_sector_data_t *uft_track_find_sector(
    uft_track_data_t *track,
    uint8_t sector_id
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_XDF_ADAPTER_H */
