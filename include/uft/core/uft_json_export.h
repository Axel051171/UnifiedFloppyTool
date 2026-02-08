/**
 * @file uft_json_export.h
 * @brief JSON Diagnostic Export for UFT
 * 
 * Provides machine-readable diagnostic output in JSON format
 * for integration with external tools, GUIs, and analysis pipelines.
 */

#ifndef UFT_JSON_EXPORT_H
#define UFT_JSON_EXPORT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * JSON Writer Context
 * ============================================================================ */

/**
 * @brief JSON output destination types
 */
typedef enum {
    UFT_JSON_TO_BUFFER,
    UFT_JSON_TO_FILE,
    UFT_JSON_TO_CALLBACK
} uft_json_output_t;

/**
 * @brief JSON write callback
 */
typedef int (*uft_json_write_fn)(const char *data, size_t len, void *user_data);

/**
 * @brief JSON writer context
 */
typedef struct {
    uft_json_output_t output_type;
    
    /* Buffer output */
    char *buffer;
    size_t buffer_size;
    size_t buffer_pos;
    
    /* File output */
    FILE *file;
    
    /* Callback output */
    uft_json_write_fn callback;
    void *callback_data;
    
    /* State */
    int indent_level;
    bool pretty_print;
    bool first_element;
    int error;
} uft_json_writer_t;

/* ============================================================================
 * Diagnostic Report Types
 * ============================================================================ */

/**
 * @brief Track diagnostic info
 */
typedef struct {
    uint8_t  track;
    uint8_t  head;
    uint32_t data_bits;
    uint32_t flux_transitions;
    double   rpm;
    double   bitrate;
    uint8_t  sectors_found;
    uint8_t  sectors_good;
    uint8_t  sectors_bad;
    uint8_t  quality;            /**< 0-100% */
    bool     has_weak_bits;
    uint32_t weak_bit_count;
    char     encoding[16];
    char     protection[32];
} uft_track_diag_t;

/**
 * @brief Sector diagnostic info
 */
typedef struct {
    uint8_t  track;
    uint8_t  head;
    uint8_t  sector;
    uint16_t size;
    uint8_t  status;             /**< 0=OK, 1=CRC error, 2=missing, etc. */
    uint8_t  confidence;         /**< 0-100% */
    uint32_t header_crc;
    uint32_t data_crc;
    bool     header_ok;
    bool     data_ok;
    double   timing_deviation;   /**< Timing deviation in % */
} uft_sector_diag_t;

/**
 * @brief Full disk diagnostic report
 */
typedef struct {
    /* Basic info */
    char     filename[256];
    char     format[32];
    char     encoding[16];
    uint32_t file_size;
    
    /* Geometry */
    uint8_t  tracks;
    uint8_t  sides;
    uint16_t sectors_per_track;
    uint16_t sector_size;
    uint32_t total_sectors;
    
    /* Analysis results */
    uint32_t sectors_good;
    uint32_t sectors_bad;
    uint32_t sectors_missing;
    double   overall_quality;    /**< 0-100% */
    
    /* Protection */
    char     protection[64];
    bool     has_protection;
    
    /* Checksums */
    uint32_t crc32;
    char     md5[33];
    char     sha1[41];
    
    /* Track diagnostics */
    uft_track_diag_t *tracks_diag;
    size_t track_count;
    
    /* Sector diagnostics (optional, can be NULL) */
    uft_sector_diag_t *sectors_diag;
    size_t sector_count;
    
} uft_disk_diag_t;

/* ============================================================================
 * JSON Writer Functions
 * ============================================================================ */

/**
 * @brief Initialize JSON writer for buffer output
 */
void uft_json_init_buffer(
    uft_json_writer_t *writer,
    char *buffer,
    size_t size
);

/**
 * @brief Initialize JSON writer for file output
 */
void uft_json_init_file(
    uft_json_writer_t *writer,
    FILE *file
);

/**
 * @brief Initialize JSON writer for callback output
 */
void uft_json_init_callback(
    uft_json_writer_t *writer,
    uft_json_write_fn callback,
    void *user_data
);

/**
 * @brief Set pretty print mode
 */
void uft_json_set_pretty(uft_json_writer_t *writer, bool pretty);

/**
 * @brief Write raw string
 */
int uft_json_write_raw(uft_json_writer_t *writer, const char *str);

/**
 * @brief Start JSON object
 */
int uft_json_object_start(uft_json_writer_t *writer);

/**
 * @brief End JSON object
 */
int uft_json_object_end(uft_json_writer_t *writer);

/**
 * @brief Start JSON array
 */
int uft_json_array_start(uft_json_writer_t *writer);

/**
 * @brief End JSON array
 */
int uft_json_array_end(uft_json_writer_t *writer);

/**
 * @brief Write object key
 */
int uft_json_key(uft_json_writer_t *writer, const char *key);

/**
 * @brief Write string value
 */
int uft_json_string(uft_json_writer_t *writer, const char *value);

/**
 * @brief Write integer value
 */
int uft_json_int(uft_json_writer_t *writer, int64_t value);

/**
 * @brief Write unsigned integer value
 */
int uft_json_uint(uft_json_writer_t *writer, uint64_t value);

/**
 * @brief Write floating point value
 */
int uft_json_double(uft_json_writer_t *writer, double value, int precision);

/**
 * @brief Write boolean value
 */
int uft_json_bool(uft_json_writer_t *writer, bool value);

/**
 * @brief Write null value
 */
int uft_json_null(uft_json_writer_t *writer);

/**
 * @brief Write key-value pair (string)
 */
int uft_json_kv_string(uft_json_writer_t *writer, const char *key, const char *value);

/**
 * @brief Write key-value pair (integer)
 */
int uft_json_kv_int(uft_json_writer_t *writer, const char *key, int64_t value);

/**
 * @brief Write key-value pair (unsigned integer)
 */
int uft_json_kv_uint(uft_json_writer_t *writer, const char *key, uint64_t value);

/**
 * @brief Write key-value pair (double)
 */
int uft_json_kv_double(uft_json_writer_t *writer, const char *key, double value, int precision);

/**
 * @brief Write key-value pair (boolean)
 */
int uft_json_kv_bool(uft_json_writer_t *writer, const char *key, bool value);

/**
 * @brief Get bytes written
 */
size_t uft_json_bytes_written(const uft_json_writer_t *writer);

/**
 * @brief Check for errors
 */
bool uft_json_has_error(const uft_json_writer_t *writer);

/* ============================================================================
 * Diagnostic Export Functions
 * ============================================================================ */

/**
 * @brief Export track diagnostics to JSON
 */
int uft_json_export_track_diag(
    uft_json_writer_t *writer,
    const uft_track_diag_t *diag
);

/**
 * @brief Export sector diagnostics to JSON
 */
int uft_json_export_sector_diag(
    uft_json_writer_t *writer,
    const uft_sector_diag_t *diag
);

/**
 * @brief Export full disk diagnostics to JSON
 */
int uft_json_export_disk_diag(
    uft_json_writer_t *writer,
    const uft_disk_diag_t *diag
);

/**
 * @brief Export disk diagnostics to file
 */
int uft_json_export_disk_to_file(
    const uft_disk_diag_t *diag,
    const char *filename
);

/**
 * @brief Export disk diagnostics to buffer
 */
size_t uft_json_export_disk_to_buffer(
    const uft_disk_diag_t *diag,
    char *buffer,
    size_t size
);

/* ============================================================================
 * Diagnostic Report Creation
 * ============================================================================ */

/**
 * @brief Initialize disk diagnostics structure
 */
void uft_disk_diag_init(uft_disk_diag_t *diag);

/**
 * @brief Free disk diagnostics resources
 */
void uft_disk_diag_free(uft_disk_diag_t *diag);

/**
 * @brief Allocate track diagnostics array
 */
int uft_disk_diag_alloc_tracks(uft_disk_diag_t *diag, size_t count);

/**
 * @brief Allocate sector diagnostics array
 */
int uft_disk_diag_alloc_sectors(uft_disk_diag_t *diag, size_t count);

/**
 * @brief Calculate summary statistics
 */
void uft_disk_diag_calc_summary(uft_disk_diag_t *diag);

/* ============================================================================
 * Quick Export Functions
 * ============================================================================ */

/**
 * @brief Quick export: error report
 */
size_t uft_json_error_report(
    char *buffer,
    size_t size,
    int error_code,
    const char *error_msg,
    const char *filename,
    int track,
    int sector
);

/**
 * @brief Quick export: progress report
 */
size_t uft_json_progress_report(
    char *buffer,
    size_t size,
    int current,
    int total,
    const char *operation,
    double elapsed_sec
);

/**
 * @brief Quick export: completion report
 */
size_t uft_json_completion_report(
    char *buffer,
    size_t size,
    bool success,
    const char *operation,
    uint32_t items_processed,
    uint32_t items_failed,
    double elapsed_sec
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_JSON_EXPORT_H */
