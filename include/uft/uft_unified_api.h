/**
 * @file uft_unified_api.h
 * @brief UFT Unified High-Level API
 * 
 * Simple API for common floppy disk operations:
 * - Load disk images (ADF, D64, DSK, IMG, etc.)
 * - Extract files
 * - Convert between formats
 * - Analyze flux captures
 * 
 * @version 5.28.0 GOD MODE
 */

#ifndef UFT_UNIFIED_API_H
#define UFT_UNIFIED_API_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Opaque Types
 * ============================================================================ */

typedef struct uft_context uft_context_t;
typedef struct uft_image uft_image_t;
typedef struct uft_file uft_file_t;
typedef struct uft_dir uft_dir_t;

/* ============================================================================
 * Error Handling
 * ============================================================================ */

typedef enum {
    UFT_SUCCESS = 0,
    UFT_ERR_INVALID_ARG,
    UFT_ERR_NO_MEMORY,
    UFT_ERR_IO,
    UFT_ERR_NOT_FOUND,
    UFT_ERR_FORMAT,
    UFT_ERR_UNSUPPORTED,
    UFT_ERR_CRC,
    UFT_ERR_CORRUPT,
    UFT_ERR_PERMISSION,
    UFT_ERR_INTERNAL
} uft_status_t;

/**
 * @brief Get error message for status code
 */
const char* uft_strerror(uft_status_t status);

/**
 * @brief Get last error message (detailed)
 */
const char* uft_get_last_error(uft_context_t *ctx);

/* ============================================================================
 * Context Management
 * ============================================================================ */

/**
 * @brief Create UFT context
 * 
 * The context holds global state and configuration.
 * Create one context per application.
 */
uft_context_t* uft_create(void);

/**
 * @brief Destroy UFT context
 */
void uft_destroy(uft_context_t *ctx);

/**
 * @brief Set option
 */
uft_status_t uft_set_option(uft_context_t *ctx, const char *key, const char *value);

/**
 * @brief Get option
 */
const char* uft_get_option(uft_context_t *ctx, const char *key);

/* ============================================================================
 * Image Loading
 * ============================================================================ */

/**
 * @brief Image type (auto-detected)
 */
typedef enum {
    UFT_IMAGE_UNKNOWN = 0,
    UFT_IMAGE_SECTOR,       /**< Sector-based (ADF, D64, IMG, etc.) */
    UFT_IMAGE_FLUX,         /**< Flux-based (SCP, A2R, KryoFlux, etc.) */
    UFT_IMAGE_BITSTREAM,    /**< Bitstream (HFE, etc.) */
    UFT_IMAGE_ARCHIVE       /**< Archive (DMS, etc.) */
} uft_image_type_t;

/**
 * @brief Image information
 */
typedef struct {
    uft_image_type_t type;
    const char *format_name;    /**< "ADF", "D64", "SCP", etc. */
    const char *platform_name;  /**< "Amiga", "C64", "IBM PC", etc. */
    const char *fs_name;        /**< "OFS", "FAT12", "CBM DOS", etc. */
    
    size_t tracks;              /**< Number of tracks */
    size_t heads;               /**< Number of heads */
    size_t sectors_per_track;   /**< Sectors per track (0 if variable) */
    size_t sector_size;         /**< Bytes per sector */
    size_t total_size;          /**< Total size in bytes */
    
    bool write_protected;
    bool has_errors;
    size_t error_count;
    
    char volume_name[64];
} uft_image_info_t;

/**
 * @brief Load disk image from file
 * 
 * Auto-detects format based on extension and content.
 * 
 * @param ctx UFT context
 * @param path Path to image file
 * @param image Output image handle
 * @return Status code
 */
uft_status_t uft_load(uft_context_t *ctx, const char *path, uft_image_t **image);

/**
 * @brief Load disk image from memory
 */
uft_status_t uft_load_memory(uft_context_t *ctx, const uint8_t *data, 
                             size_t size, const char *format_hint,
                             uft_image_t **image);

/**
 * @brief Close image
 */
void uft_close(uft_image_t *image);

/**
 * @brief Get image information
 */
uft_status_t uft_get_info(uft_image_t *image, uft_image_info_t *info);

/* ============================================================================
 * Filesystem Operations
 * ============================================================================ */

/**
 * @brief Directory entry
 */
typedef struct {
    char name[256];
    size_t size;
    bool is_dir;
    bool is_hidden;
    bool is_protected;
    uint32_t modified;      /**< Unix timestamp */
    char type_info[32];     /**< File type info (platform specific) */
} uft_entry_t;

/**
 * @brief Open directory
 */
uft_status_t uft_opendir(uft_image_t *image, const char *path, uft_dir_t **dir);

/**
 * @brief Read next directory entry
 */
uft_status_t uft_readdir(uft_dir_t *dir, uft_entry_t *entry);

/**
 * @brief Close directory
 */
void uft_closedir(uft_dir_t *dir);

/**
 * @brief List all files (convenience function)
 */
uft_status_t uft_list_files(uft_image_t *image, const char *path,
                            uft_entry_t **entries, size_t *count);

/**
 * @brief Free file list
 */
void uft_free_file_list(uft_entry_t *entries, size_t count);

/* ============================================================================
 * File Operations
 * ============================================================================ */

/**
 * @brief Open file for reading
 */
uft_status_t uft_fopen(uft_image_t *image, const char *path, uft_file_t **file);

/**
 * @brief Read from file
 */
uft_status_t uft_fread(uft_file_t *file, void *buffer, size_t size, size_t *read);

/**
 * @brief Seek in file
 */
uft_status_t uft_fseek(uft_file_t *file, long offset, int whence);

/**
 * @brief Get file position
 */
long uft_ftell(uft_file_t *file);

/**
 * @brief Get file size
 */
size_t uft_fsize(uft_file_t *file);

/**
 * @brief Close file
 */
void uft_fclose(uft_file_t *file);

/**
 * @brief Extract file to disk (convenience function)
 */
uft_status_t uft_extract(uft_image_t *image, const char *src_path, 
                         const char *dest_path);

/**
 * @brief Extract all files to directory
 */
uft_status_t uft_extract_all(uft_image_t *image, const char *dest_dir);

/**
 * @brief Read entire file into memory
 */
uft_status_t uft_read_file(uft_image_t *image, const char *path,
                           uint8_t **data, size_t *size);

/**
 * @brief Free file data
 */
void uft_free_data(uint8_t *data);

/* ============================================================================
 * Raw Access
 * ============================================================================ */

/**
 * @brief Read raw sector
 */
uft_status_t uft_read_sector(uft_image_t *image, 
                             size_t track, size_t head, size_t sector,
                             uint8_t *buffer, size_t buffer_size);

/**
 * @brief Read raw track
 */
uft_status_t uft_read_track(uft_image_t *image,
                            size_t track, size_t head,
                            uint8_t *buffer, size_t buffer_size,
                            size_t *actual_size);

/**
 * @brief Get track info
 */
typedef struct {
    size_t sector_count;
    size_t sector_size;
    size_t gap_size;
    bool has_errors;
    size_t error_sectors;
    double rpm;
    const char *encoding;
} uft_track_info_t;

uft_status_t uft_get_track_info(uft_image_t *image, 
                                size_t track, size_t head,
                                uft_track_info_t *info);

/* ============================================================================
 * Conversion
 * ============================================================================ */

/**
 * @brief Convert image to another format
 */
uft_status_t uft_convert(uft_image_t *image, const char *dest_path,
                         const char *format);

/**
 * @brief Get list of supported output formats
 */
const char** uft_get_output_formats(size_t *count);

/**
 * @brief Check if format conversion is supported
 */
bool uft_can_convert(uft_image_t *image, const char *format);

/* ============================================================================
 * Flux Analysis
 * ============================================================================ */

/**
 * @brief Flux track info
 */
typedef struct {
    size_t revolution_count;
    double index_time_ms;
    double rpm;
    size_t flux_count;
    double bitcell_us;
    const char *detected_encoding;
    uint8_t confidence;
} uft_flux_info_t;

/**
 * @brief Get flux track info
 */
uft_status_t uft_get_flux_info(uft_image_t *image,
                               size_t track, size_t head,
                               uft_flux_info_t *info);

/**
 * @brief Decode flux to sectors
 */
uft_status_t uft_decode_flux(uft_image_t *image,
                             size_t track, size_t head,
                             uint8_t *sector_buffer,
                             size_t buffer_size,
                             size_t *sectors_found);

/* ============================================================================
 * Diagnostics
 * ============================================================================ */

/**
 * @brief Verify image integrity
 */
typedef struct {
    bool passed;
    size_t total_sectors;
    size_t good_sectors;
    size_t bad_sectors;
    size_t missing_sectors;
    char details[1024];
} uft_verify_result_t;

uft_status_t uft_verify(uft_image_t *image, uft_verify_result_t *result);

/**
 * @brief Get detailed track report
 */
uft_status_t uft_analyze_track(uft_image_t *image,
                               size_t track, size_t head,
                               char *report, size_t report_size);

/* ============================================================================
 * Format Detection
 * ============================================================================ */

/**
 * @brief Detect format from file
 */
uft_status_t uft_detect_format(const char *path, 
                               char *format, size_t format_size,
                               uint8_t *confidence);

/**
 * @brief Detect format from memory
 */
uft_status_t uft_detect_format_memory(const uint8_t *data, size_t size,
                                      char *format, size_t format_size,
                                      uint8_t *confidence);

/**
 * @brief Get list of supported input formats
 */
const char** uft_get_input_formats(size_t *count);

/**
 * @brief Get format description
 */
const char* uft_get_format_description(const char *format);

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Progress callback
 */
typedef void (*uft_progress_cb)(void *user_data, 
                                size_t current, size_t total,
                                const char *message);

/**
 * @brief Set progress callback
 */
void uft_set_progress_callback(uft_context_t *ctx, 
                               uft_progress_cb callback,
                               void *user_data);

/**
 * @brief Log callback
 */
typedef void (*uft_log_cb)(void *user_data, int level, const char *message);

/**
 * @brief Set log callback
 */
void uft_set_log_callback(uft_context_t *ctx,
                          uft_log_cb callback,
                          void *user_data);

/* ============================================================================
 * Version Info
 * ============================================================================ */

/**
 * @brief Get library version string
 */
const char* uft_version(void);

/**
 * @brief Get version components
 */
void uft_version_info(int *major, int *minor, int *patch);

/**
 * @brief Get build info
 */
const char* uft_build_info(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_UNIFIED_API_H */
