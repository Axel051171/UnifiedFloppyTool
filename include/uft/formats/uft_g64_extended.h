/**
 * @file uft_g64_extended.h
 * @brief G64 format with error map extension
 * 
 * P1-005: G64 format lacked error information
 * 
 * G64 Extended Format:
 * - Standard G64 1.2 header and data
 * - Optional extension block for error maps
 * - Backward compatible with standard G64 readers
 */

#ifndef UFT_G64_EXTENDED_H
#define UFT_G64_EXTENDED_H

#include "uft/core/uft_unified_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* G64 Constants */
#define G64_SIGNATURE       "GCR-1541"
#define G64_SIGNATURE_EXT   "GCR-1541E"  /* Extended signature */
#define G64_VERSION         0
#define G64_MAX_TRACKS      84
#define G64_MAX_TRACK_SIZE  7928

/* G64 Extended Magic */
#define G64_EXT_MAGIC       "UFTX"
#define G64_EXT_VERSION     0x01

/* Error types for G64 extension */
typedef enum {
    G64_ERR_NONE = 0x00,
    G64_ERR_CRC = 0x01,
    G64_ERR_HEADER_CRC = 0x02,
    G64_ERR_DATA_CRC = 0x03,
    G64_ERR_NO_SYNC = 0x04,
    G64_ERR_NO_HEADER = 0x05,
    G64_ERR_NO_DATA = 0x06,
    G64_ERR_ID_MISMATCH = 0x07,
    G64_ERR_SECTOR_COUNT = 0x08,
    G64_ERR_GCR_INVALID = 0x09,
    G64_ERR_TIMING = 0x0A,
    G64_ERR_WEAK_BITS = 0x0B,
    G64_ERR_UNKNOWN = 0xFF,
} g64_error_type_t;

/**
 * @brief G64 sector error entry
 */
typedef struct {
    uint8_t track;           /**< Track number (0-83) */
    uint8_t sector;          /**< Sector number (0-20) */
    uint8_t error_type;      /**< g64_error_type_t */
    uint8_t confidence;      /**< Confidence 0-255 */
    uint16_t bit_position;   /**< Position in track (optional) */
    uint16_t reserved;       /**< Padding */
} g64_error_entry_t;

/**
 * @brief G64 extended error map
 */
typedef struct {
    char magic[4];           /**< "UFTX" */
    uint8_t version;         /**< Extension version */
    uint8_t flags;           /**< Flags */
    uint16_t error_count;    /**< Number of errors */
    g64_error_entry_t *errors; /**< Error array */
    
    /* Per-track metadata */
    struct {
        uint8_t encoding;    /**< UFT_ENC_* */
        uint8_t speed_zone;  /**< 0-3 */
        uint8_t quality;     /**< 0-100 */
        uint8_t flags;       /**< Track flags */
    } track_meta[G64_MAX_TRACKS];
    
} g64_error_map_t;

/* Flags */
#define G64_FLAG_HAS_ERRORS     0x01
#define G64_FLAG_HAS_TIMING     0x02
#define G64_FLAG_HAS_WEAK_BITS  0x04
#define G64_FLAG_MULTI_REV      0x08

/**
 * @brief G64 extended write options
 */
typedef struct {
    bool include_error_map;  /**< Include error extension */
    bool include_metadata;   /**< Include track metadata */
    uint8_t version;         /**< G64 version (0=standard) */
    bool force_84_tracks;    /**< Always write 84 tracks */
} g64_write_options_t;

/**
 * @brief G64 read result
 */
typedef struct {
    bool success;
    uft_error_t error;
    
    uint8_t tracks;
    uint8_t max_track_size;
    bool has_extension;
    
    /* Error summary */
    uint16_t total_errors;
    uint16_t crc_errors;
    uint16_t header_errors;
    uint16_t data_errors;
    
} g64_read_result_t;

/* ============================================================================
 * Error Map Functions
 * ============================================================================ */

/**
 * @brief Initialize error map
 */
void g64_error_map_init(g64_error_map_t *map);

/**
 * @brief Free error map
 */
void g64_error_map_free(g64_error_map_t *map);

/**
 * @brief Add error to map
 */
int g64_error_map_add(g64_error_map_t *map,
                      uint8_t track, uint8_t sector,
                      g64_error_type_t type, uint8_t confidence);

/**
 * @brief Get error for sector
 */
g64_error_entry_t* g64_error_map_get(const g64_error_map_t *map,
                                     uint8_t track, uint8_t sector);

/**
 * @brief Count errors for track
 */
int g64_error_map_count_track(const g64_error_map_t *map, uint8_t track);

/* ============================================================================
 * G64 Extended I/O
 * ============================================================================ */

/**
 * @brief Read G64 with error map
 */
uft_error_t uft_g64_read_extended(const char *path,
                                  uft_disk_image_t **out_disk,
                                  g64_error_map_t *out_errors,
                                  g64_read_result_t *result);

/**
 * @brief Write G64 with error map
 */
uft_error_t uft_g64_write_extended(const uft_disk_image_t *disk,
                                   const char *path,
                                   const g64_error_map_t *errors,
                                   const g64_write_options_t *opts);

/**
 * @brief Check if file has G64 extension
 */
bool uft_g64_has_extension(const char *path);

/**
 * @brief Read only the error map from G64
 */
uft_error_t uft_g64_read_error_map(const char *path,
                                   g64_error_map_t *out_errors);

/**
 * @brief Append error map to existing G64
 */
uft_error_t uft_g64_append_error_map(const char *path,
                                     const g64_error_map_t *errors);

/* ============================================================================
 * Conversion
 * ============================================================================ */

/**
 * @brief Build error map from disk analysis
 */
uft_error_t g64_build_error_map(const uft_disk_image_t *disk,
                                g64_error_map_t *out_map);

/**
 * @brief Apply error map to disk
 */
void g64_apply_error_map(uft_disk_image_t *disk,
                         const g64_error_map_t *map);

/**
 * @brief Initialize write options
 */
void uft_g64_write_options_init(g64_write_options_t *opts);

/**
 * @brief Get error type name
 */
const char* g64_error_type_name(g64_error_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_G64_EXTENDED_H */
