/**
 * @file uft_writer_backend.h
 * @brief Writer Backend Interface
 * 
 * P0-002: Vollständiges Writer Backend für Transaction System
 * 
 * Unterstützt:
 * - Image-Writer (ADF, D64, SCP, etc.)
 * - Hardware-Writer (Greaseweazle, FluxEngine, etc.)
 * - Memory-Writer (für Tests)
 */

#ifndef UFT_WRITER_BACKEND_H
#define UFT_WRITER_BACKEND_H

#include "uft_types.h"
#include "uft_error.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Backend Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_BACKEND_NONE = 0,
    UFT_BACKEND_IMAGE,          /* Write to disk image file */
    UFT_BACKEND_HARDWARE,       /* Write to physical hardware */
    UFT_BACKEND_MEMORY,         /* Write to memory buffer (testing) */
    UFT_BACKEND_FLUX,           /* Write flux data */
} uft_backend_type_t;

typedef enum {
    UFT_WRITE_MODE_RAW = 0,     /* Raw sector data */
    UFT_WRITE_MODE_ENCODED,     /* Already MFM/GCR encoded */
    UFT_WRITE_MODE_FLUX,        /* Flux timing data */
} uft_write_mode_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Backend Context
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct uft_writer_backend uft_writer_backend_t;

typedef struct {
    uft_backend_type_t  type;
    uft_write_mode_t    mode;
    uft_encoding_t      encoding;
    uft_format_t        format;
    
    /* Image backend options */
    const char          *image_path;
    bool                create_new;
    bool                truncate;
    
    /* Hardware backend options */
    const char          *device_path;
    int                 drive_select;
    bool                double_step;
    int                 retries;
    int                 verify_retries;
    
    /* Write options */
    bool                precomp_enable;
    int                 precomp_ns;
    uint8_t             gap3_size;
    uint8_t             fill_byte;
    
    /* Flux options */
    double              clock_rate_hz;
    double              bit_cell_ns;
} uft_writer_options_t;

/* Default options */
#define UFT_WRITER_OPTIONS_DEFAULT { \
    .type = UFT_BACKEND_IMAGE, \
    .mode = UFT_WRITE_MODE_RAW, \
    .encoding = UFT_ENC_MFM, \
    .format = UFT_FORMAT_UNKNOWN, \
    .image_path = NULL, \
    .create_new = false, \
    .truncate = false, \
    .device_path = NULL, \
    .drive_select = 0, \
    .double_step = false, \
    .retries = 3, \
    .verify_retries = 2, \
    .precomp_enable = true, \
    .precomp_ns = 140, \
    .gap3_size = 0, \
    .fill_byte = 0x4E, \
    .clock_rate_hz = 24000000.0, \
    .bit_cell_ns = 2000.0, \
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Statistics
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    int                 tracks_written;
    int                 tracks_verified;
    int                 tracks_failed;
    int                 sectors_written;
    int                 sectors_failed;
    int                 verify_errors;
    int                 retry_count;
    size_t              bytes_written;
    double              elapsed_ms;
} uft_writer_stats_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Callback Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef void (*uft_writer_progress_fn)(int cylinder, int head, int percent,
                                        const char *status, void *user);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Backend Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create writer backend
 * @param options Backend configuration
 * @return Backend handle or NULL on error
 */
uft_writer_backend_t* uft_writer_create(const uft_writer_options_t *options);

/**
 * @brief Destroy writer backend
 */
void uft_writer_destroy(uft_writer_backend_t *backend);

/**
 * @brief Open backend for writing
 * @return UFT_OK on success
 */
uft_error_t uft_writer_open(uft_writer_backend_t *backend);

/**
 * @brief Close backend
 */
void uft_writer_close(uft_writer_backend_t *backend);

/**
 * @brief Check if backend is ready
 */
bool uft_writer_is_ready(const uft_writer_backend_t *backend);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Write Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Write a complete track
 * @param backend Backend handle
 * @param cylinder Cylinder number
 * @param head Head number (0 or 1)
 * @param data Track data
 * @param size Data size in bytes
 * @return UFT_OK on success
 */
uft_error_t uft_writer_write_track(uft_writer_backend_t *backend,
                                    uint8_t cylinder, uint8_t head,
                                    const uint8_t *data, size_t size);

/**
 * @brief Write a single sector
 * @param backend Backend handle
 * @param cylinder Cylinder number
 * @param head Head number
 * @param sector Sector number
 * @param data Sector data
 * @param size Data size
 * @return UFT_OK on success
 */
uft_error_t uft_writer_write_sector(uft_writer_backend_t *backend,
                                     uint8_t cylinder, uint8_t head, uint8_t sector,
                                     const uint8_t *data, size_t size);

/**
 * @brief Write flux data to track
 * @param backend Backend handle
 * @param cylinder Cylinder number
 * @param head Head number
 * @param flux_times Array of flux transition times (nanoseconds)
 * @param count Number of transitions
 * @return UFT_OK on success
 */
uft_error_t uft_writer_write_flux(uft_writer_backend_t *backend,
                                   uint8_t cylinder, uint8_t head,
                                   const double *flux_times, size_t count);

/**
 * @brief Format a track (erase and write structure)
 * @param backend Backend handle
 * @param cylinder Cylinder number
 * @param head Head number
 * @param sectors_per_track Number of sectors
 * @param sector_size Bytes per sector
 * @return UFT_OK on success
 */
uft_error_t uft_writer_format_track(uft_writer_backend_t *backend,
                                     uint8_t cylinder, uint8_t head,
                                     int sectors_per_track, int sector_size);

/**
 * @brief Erase a track (write blank flux)
 * @return UFT_OK on success
 */
uft_error_t uft_writer_erase_track(uft_writer_backend_t *backend,
                                    uint8_t cylinder, uint8_t head);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Verify Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Verify written track data
 */
uft_error_t uft_writer_verify_track(uft_writer_backend_t *backend,
                                     uint8_t cylinder, uint8_t head,
                                     const uint8_t *expected, size_t size);

/**
 * @brief Verify written sector
 */
uft_error_t uft_writer_verify_sector(uft_writer_backend_t *backend,
                                      uint8_t cylinder, uint8_t head, uint8_t sector,
                                      const uint8_t *expected, size_t size);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Read-back Operations (for verify)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read track back for verification
 */
uft_error_t uft_writer_read_track(uft_writer_backend_t *backend,
                                   uint8_t cylinder, uint8_t head,
                                   uint8_t *buffer, size_t size);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Set progress callback
 */
void uft_writer_set_progress(uft_writer_backend_t *backend,
                              uft_writer_progress_fn callback, void *user);

/**
 * @brief Get statistics
 */
void uft_writer_get_stats(const uft_writer_backend_t *backend,
                           uft_writer_stats_t *stats);

/**
 * @brief Reset statistics
 */
void uft_writer_reset_stats(uft_writer_backend_t *backend);

/**
 * @brief Get last error message
 */
const char* uft_writer_get_error(const uft_writer_backend_t *backend);

/**
 * @brief Get backend type name
 */
const char* uft_backend_type_name(uft_backend_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_WRITER_BACKEND_H */
