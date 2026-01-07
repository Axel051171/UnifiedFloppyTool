/**
 * @file uft_public_api.h
 * @brief Unified Public API for UFT
 * 
 * P1-H05: Single-include header for all public UFT functionality
 * 
 * USAGE:
 *   #include <uft/uft_public_api.h>
 * 
 * This header provides stable, versioned APIs that:
 * - Are guaranteed to be backward compatible within major versions
 * - Have well-defined ownership semantics
 * - Use consistent error handling
 * - Are suitable for GUI, CLI, and library consumers
 */

#ifndef UFT_PUBLIC_API_H
#define UFT_PUBLIC_API_H

/* API Version */
#define UFT_API_VERSION_MAJOR 1
#define UFT_API_VERSION_MINOR 0
#define UFT_API_VERSION_PATCH 0
#define UFT_API_VERSION ((UFT_API_VERSION_MAJOR << 16) | \
                         (UFT_API_VERSION_MINOR << 8) | \
                         UFT_API_VERSION_PATCH)

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * SECTION 1: CORE TYPES
 * ============================================================================ */

/* Error codes - canonical source */
#include "uft/core/uft_unified_types.h"

/* Forward declarations for opaque types
 * Note: uft_disk_image_t, uft_track_t, uft_sector_t are defined in uft_unified_types.h
 */
#ifndef UFT_CONTEXT_T_DEFINED
#define UFT_CONTEXT_T_DEFINED
typedef struct uft_context uft_context_t;
#endif

/* ============================================================================
 * SECTION 2: INITIALIZATION
 * ============================================================================ */

/**
 * @brief Initialize UFT library
 * @return UFT_OK on success
 * 
 * Must be called before any other UFT functions.
 * Thread-safe, can be called multiple times.
 */
uft_error_t uft_init(void);

/**
 * @brief Cleanup UFT library
 * 
 * Call when done with UFT. Frees global resources.
 */
void uft_cleanup(void);

/**
 * @brief Get UFT version string
 */
const char* uft_version(void);

/**
 * @brief Get UFT API version
 */
uint32_t uft_api_version(void);

/* ============================================================================
 * SECTION 3: FORMAT DETECTION & INFO
 * ============================================================================ */

/**
 * @brief Detect format of a file
 * @param path File path
 * @param out_format Output: detected format ID
 * @param out_confidence Output: confidence 0-100 (optional)
 * @return UFT_OK on success
 */
uft_error_t uft_detect_format(const char *path,
                              uft_format_id_t *out_format,
                              uint8_t *out_confidence);

/**
 * @brief Detect format from memory
 */
uft_error_t uft_detect_format_mem(const uint8_t *data, size_t size,
                                  uft_format_id_t *out_format,
                                  uint8_t *out_confidence);

/**
 * @brief Get format name string
 */
const char* uft_get_format_name(uft_format_id_t format);

/**
 * @brief Get format capabilities
 */
typedef struct {
    uft_format_id_t format;
    const char *name;
    const char *extension;
    
    bool can_read;
    bool can_write;
    bool can_repair;
    
    bool supports_timing;
    bool supports_weak_bits;
    bool supports_long_tracks;
    bool supports_error_map;
    bool supports_multi_rev;
    
} uft_format_info_t;

uft_error_t uft_get_format_info(uft_format_id_t format,
                                uft_format_info_t *out_info);

/**
 * @brief List all supported formats
 * @param formats Output array
 * @param max_formats Array size
 * @return Number of formats written
 */
int uft_list_formats(uft_format_id_t *formats, size_t max_formats);

/* ============================================================================
 * SECTION 4: DISK IMAGE I/O
 * ============================================================================ */

/**
 * @brief Read options
 */
typedef struct {
    bool analyze;              /**< Run analysis after read */
    bool detect_protection;    /**< Detect copy protection */
    bool preserve_errors;      /**< Keep error information */
    uint8_t max_retries;       /**< Retry count for hardware */
} uft_read_options_t;

/**
 * @brief Initialize read options with defaults
 */
void uft_read_options_init(uft_read_options_t *opts);

/**
 * @brief Read disk image from file
 * @param path File path
 * @param out_disk Output: disk image (caller must free with uft_disk_free)
 * @param opts Read options (NULL for defaults)
 * @return UFT_OK on success
 */
uft_error_t uft_read(const char *path,
                     uft_disk_image_t **out_disk,
                     const uft_read_options_t *opts);

/**
 * @brief Read disk image from memory
 */
uft_error_t uft_read_mem(const uint8_t *data, size_t size,
                         uft_disk_image_t **out_disk,
                         const uft_read_options_t *opts);

/**
 * @brief Write options
 */
typedef struct {
    uft_format_id_t format;    /**< Output format (0 = same as input) */
    bool verify;               /**< Verify after write */
    bool preserve_errors;      /**< Preserve error flags */
    bool compress;             /**< Use compression if available */
} uft_write_options_t;

/**
 * @brief Initialize write options with defaults
 */
void uft_write_options_init(uft_write_options_t *opts);

/**
 * @brief Write disk image to file
 * @param disk Disk image
 * @param path Output path
 * @param opts Write options (NULL for defaults)
 * @return UFT_OK on success
 */
uft_error_t uft_write(const uft_disk_image_t *disk,
                      const char *path,
                      const uft_write_options_t *opts);

/**
 * @brief Write disk image to memory
 * @param disk Disk image
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @param out_size Output: actual size written
 * @param opts Write options (NULL for defaults)
 * @return UFT_OK on success
 */
uft_error_t uft_write_mem(const uft_disk_image_t *disk,
                          uint8_t *buffer, size_t buffer_size,
                          size_t *out_size,
                          const uft_write_options_t *opts);

/* ============================================================================
 * SECTION 5: DISK IMAGE ACCESS
 * ============================================================================ */

/**
 * @brief Get disk geometry
 */
typedef struct {
    uint16_t tracks;
    uint8_t heads;
    uint8_t sectors_per_track;  /* 0 = variable */
    uint16_t bytes_per_sector;  /* 0 = variable */
    uft_format_id_t format;
} uft_geometry_t;

uft_error_t uft_get_geometry(const uft_disk_image_t *disk,
                             uft_geometry_t *out_geom);

/**
 * @brief Get track (borrowed reference)
 * @note Do NOT free the returned pointer
 */
uft_track_t* uft_get_track(const uft_disk_image_t *disk,
                           uint16_t track, uint8_t head);

/**
 * @brief Get sector data (borrowed reference)
 * @note Do NOT free the returned pointer
 */
const uint8_t* uft_get_sector_data(const uft_disk_image_t *disk,
                                   uint16_t track, uint8_t head,
                                   uint8_t sector,
                                   size_t *out_size);

/**
 * @brief Set sector data
 */
uft_error_t uft_set_sector_data(uft_disk_image_t *disk,
                                uint16_t track, uint8_t head,
                                uint8_t sector,
                                const uint8_t *data, size_t size);

/* ============================================================================
 * SECTION 6: ANALYSIS & DIAGNOSTICS
 * ============================================================================ */

/**
 * @brief Disk analysis result
 */
typedef struct {
    bool success;
    
    /* Geometry */
    uft_geometry_t geometry;
    
    /* Quality metrics */
    uint16_t total_sectors;
    uint16_t valid_sectors;
    uint16_t error_sectors;
    float quality_percent;
    
    /* Errors */
    uint16_t crc_errors;
    uint16_t missing_sectors;
    uint16_t weak_bit_sectors;
    
    /* Protection */
    bool has_protection;
    uft_protection_t protection_type;
    uint8_t protection_confidence;
    
    /* Filesystem */
    bool has_filesystem;
    char filesystem_type[32];
    char volume_name[64];
    
} uft_analysis_t;

/**
 * @brief Analyze disk image
 */
uft_error_t uft_analyze(const uft_disk_image_t *disk,
                        uft_analysis_t *out_analysis);

/**
 * @brief Get diagnostic info for track
 */
typedef struct {
    uint16_t track;
    uint8_t head;
    
    uint8_t sectors_found;
    uint8_t sectors_valid;
    uint8_t encoding;
    uint8_t quality;
    
    bool has_errors;
    bool has_weak_bits;
    bool has_protection;
    
    const char *diagnosis;  /* Human-readable diagnosis */
    
} uft_track_diag_t;

uft_error_t uft_diagnose_track(const uft_disk_image_t *disk,
                               uint16_t track, uint8_t head,
                               uft_track_diag_t *out_diag);

/* ============================================================================
 * SECTION 7: CONVERSION
 * ============================================================================ */

/**
 * @brief Convert between formats
 * @param input_path Input file
 * @param output_path Output file
 * @param output_format Target format (0 = auto from extension)
 * @return UFT_OK on success
 */
uft_error_t uft_convert(const char *input_path,
                        const char *output_path,
                        uft_format_id_t output_format);

/**
 * @brief Convert disk in memory
 */
uft_error_t uft_convert_disk(const uft_disk_image_t *src,
                             uft_format_id_t target_format,
                             uft_disk_image_t **out_dst);

/* ============================================================================
 * SECTION 8: COPY & RECOVERY
 * ============================================================================ */

/**
 * @brief Copy options
 */
typedef struct {
    bool preserve_protection;  /**< Preserve copy protection */
    bool preserve_timing;      /**< Preserve timing data */
    bool preserve_weak_bits;   /**< Preserve weak bits */
    bool use_multi_rev;        /**< Use multi-revision reads */
    uint8_t max_retries;       /**< Maximum retries */
    uint8_t min_confidence;    /**< Minimum confidence threshold */
} uft_copy_options_t;

/**
 * @brief Initialize copy options
 */
void uft_copy_options_init(uft_copy_options_t *opts);

/**
 * @brief Copy disk with protection awareness
 */
uft_error_t uft_copy(const uft_disk_image_t *src,
                     uft_disk_image_t **out_dst,
                     const uft_copy_options_t *opts);

/**
 * @brief Recovery options
 */
typedef struct {
    enum {
        UFT_RECOVERY_SAFE,       /**< Safe: only use verified data */
        UFT_RECOVERY_AGGRESSIVE, /**< Aggressive: interpolate missing */
        UFT_RECOVERY_FORENSIC    /**< Forensic: preserve all with metadata */
    } mode;
    
    uint8_t max_retries;
    bool use_multi_rev;
    float confidence_threshold;
    
} uft_recovery_options_t;

/**
 * @brief Initialize recovery options
 */
void uft_recovery_options_init(uft_recovery_options_t *opts);

/**
 * @brief Attempt to recover damaged disk
 */
uft_error_t uft_recover(const uft_disk_image_t *damaged,
                        uft_disk_image_t **out_recovered,
                        const uft_recovery_options_t *opts);

/* ============================================================================
 * SECTION 9: PROGRESS & CALLBACKS
 * ============================================================================ */

/**
 * @brief Progress callback
 * @param current Current step
 * @param total Total steps
 * @param message Status message
 * @param user_data User data
 * @return true to continue, false to cancel
 */
typedef bool (*uft_progress_fn)(int current, int total,
                                const char *message,
                                void *user_data);

/**
 * @brief Error callback
 */
typedef void (*uft_error_fn)(uft_error_t error,
                             const char *message,
                             void *user_data);

/**
 * @brief Set global progress callback
 */
void uft_set_progress_callback(uft_progress_fn callback, void *user_data);

/**
 * @brief Set global error callback
 */
void uft_set_error_callback(uft_error_fn callback, void *user_data);

/* ============================================================================
 * SECTION 10: MEMORY MANAGEMENT
 * ============================================================================ */

/**
 * @brief Free disk image
 * @param disk Disk to free (NULL safe)
 */
void uft_disk_free(uft_disk_image_t *disk);

/**
 * @brief Duplicate disk image
 * @param disk Source disk
 * @return New disk (caller must free) or NULL
 */
uft_disk_image_t* uft_disk_dup(const uft_disk_image_t *disk);

/**
 * @brief Get memory usage
 */
size_t uft_get_memory_usage(const uft_disk_image_t *disk);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PUBLIC_API_H */
