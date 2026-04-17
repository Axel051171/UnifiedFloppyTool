/**
 * @file uft_nibtools.h
 * @brief Nibtools Integration for Raw GCR Access
 * 
 * Nibtools allows raw GCR-level access to Commodore disk drives.
 * This module provides integration with nibtools for:
 * - Raw track reading (nibread)
 * - Raw track writing (nibwrite)
 * - Deep protection analysis
 * 
 * Requires: nibtools from http://c64preservation.com/
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#ifndef UFT_NIBTOOLS_H
#define UFT_NIBTOOLS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Maximum GCR track size (bytes) */
#define UFT_NIB_MAX_TRACK_SIZE  8192

/** NIB file magic */
#define UFT_NIB_MAGIC           "MNIB-1541-RAW"
#define UFT_NIB_MAGIC_LEN       13

/** G64 file magic */
#define UFT_G64_MAGIC           "GCR-1541"
#define UFT_G64_MAGIC_LEN       8

/*===========================================================================
 * Track Density Zones
 *===========================================================================*/

typedef enum {
    UFT_NIB_ZONE_1 = 0,     /**< Tracks 1-17: 21 sectors, 3.25ms */
    UFT_NIB_ZONE_2 = 1,     /**< Tracks 18-24: 19 sectors, 3.50ms */
    UFT_NIB_ZONE_3 = 2,     /**< Tracks 25-30: 18 sectors, 3.75ms */
    UFT_NIB_ZONE_4 = 3,     /**< Tracks 31-35+: 17 sectors, 4.00ms */
} uft_nib_zone_t;

/*===========================================================================
 * Capture Modes
 *===========================================================================*/

typedef enum {
    UFT_NIB_MODE_READ = 0,      /**< Read existing disk */
    UFT_NIB_MODE_WRITE,         /**< Write to disk */
    UFT_NIB_MODE_VERIFY,        /**< Verify after write */
    UFT_NIB_MODE_ANALYZE,       /**< Deep analysis (multiple revs) */
} uft_nib_mode_t;

/*===========================================================================
 * Data Structures
 *===========================================================================*/

typedef struct uft_nib_config_s uft_nib_config_t;

/**
 * @brief Raw track data
 */
typedef struct {
    int track;                      /**< Track number (1-42) */
    int half_track;                 /**< Half track (0 or 1) */
    
    uint8_t *gcr_data;              /**< Raw GCR bytes */
    size_t gcr_size;                /**< GCR data size */
    
    uint8_t density;                /**< Density zone (0-3) */
    uint32_t sync_offset;           /**< Sync position */
    uint32_t track_length;          /**< Total track length */
    
    /* Quality metrics */
    int sync_count;                 /**< Number of syncs found */
    int bad_gcr_count;              /**< Invalid GCR sequences */
    float track_variance;           /**< Timing variance */
    
    bool weak_bits_detected;        /**< Weak bits present */
    uint8_t *weak_mask;             /**< Weak bit positions (optional) */
} uft_nib_track_t;

/**
 * @brief Capture result callback
 */
typedef int (*uft_nib_callback_t)(const uft_nib_track_t *track, void *user);

/**
 * @brief Protection detection result
 */
typedef struct {
    bool detected;
    char protection_name[64];
    int protection_track;
    int protection_sector;
    float confidence;
    
    /* Specific protection info */
    bool has_v_max;                 /**< V-MAX! detected */
    bool has_rapidlok;              /**< RapidLok detected */
    bool has_fat_track;             /**< Fat track (extra data) */
    bool has_half_tracks;           /**< Uses half-tracks */
    bool has_density_mismatch;      /**< Density manipulation */
} uft_nib_protection_t;

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

/**
 * @brief Create nibtools configuration
 */
uft_nib_config_t* uft_nib_config_create(void);

/**
 * @brief Destroy configuration
 */
void uft_nib_config_destroy(uft_nib_config_t *cfg);

/**
 * @brief Open connection via XUM1541
 * @param device_num IEC device number (typically 8)
 */
int uft_nib_open(uft_nib_config_t *cfg, int device_num);

/**
 * @brief Close connection
 */
void uft_nib_close(uft_nib_config_t *cfg);

/**
 * @brief Check if nibtools is available
 */
bool uft_nib_is_available(void);

/*===========================================================================
 * Configuration
 *===========================================================================*/

int uft_nib_set_track_range(uft_nib_config_t *cfg, int start, int end);
int uft_nib_set_half_tracks(uft_nib_config_t *cfg, bool enable);
int uft_nib_set_retries(uft_nib_config_t *cfg, int count);
int uft_nib_set_mode(uft_nib_config_t *cfg, uft_nib_mode_t mode);

/*===========================================================================
 * Read Operations
 *===========================================================================*/

/**
 * @brief Read single track raw GCR
 */
int uft_nib_read_track(uft_nib_config_t *cfg, int track, int half,
                        uint8_t **gcr, size_t *size);

/**
 * @brief Read entire disk
 */
int uft_nib_read_disk(uft_nib_config_t *cfg, 
                       uft_nib_callback_t callback, void *user);

/**
 * @brief Read track with deep analysis (multiple revolutions)
 */
int uft_nib_analyze_track(uft_nib_config_t *cfg, int track,
                           uft_nib_track_t *result, int revolutions);

/*===========================================================================
 * Write Operations
 *===========================================================================*/

/**
 * @brief Write track raw GCR
 */
int uft_nib_write_track(uft_nib_config_t *cfg, int track, int half,
                         const uint8_t *gcr, size_t size);

/**
 * @brief Write entire disk from NIB file
 */
int uft_nib_write_disk(uft_nib_config_t *cfg, const char *nib_path);

/**
 * @brief Verify track against GCR data
 */
int uft_nib_verify_track(uft_nib_config_t *cfg, int track, int half,
                          const uint8_t *gcr, size_t size,
                          int *mismatches);

/*===========================================================================
 * Protection Detection
 *===========================================================================*/

/**
 * @brief Detect copy protection from captured tracks
 */
int uft_nib_detect_protection(uft_nib_config_t *cfg,
                               uft_nib_protection_t *result);

/**
 * @brief Analyze track for weak bits
 */
int uft_nib_analyze_weak_bits(const uint8_t **revolutions, int count,
                               size_t size, uint8_t *weak_mask,
                               int *weak_count);

/*===========================================================================
 * Format Conversion
 *===========================================================================*/

/**
 * @brief Export to NIB file
 */
int uft_nib_export_nib(uft_nib_config_t *cfg, const char *path);

/**
 * @brief Export to G64 file
 */
int uft_nib_export_g64(uft_nib_config_t *cfg, const char *path);

/**
 * @brief Export to D64 (decode sectors)
 */
int uft_nib_export_d64(uft_nib_config_t *cfg, const char *path);

/**
 * @brief Import from NIB file
 */
int uft_nib_import_nib(uft_nib_config_t *cfg, const char *path);

/**
 * @brief Import from G64 file
 */
int uft_nib_import_g64(uft_nib_config_t *cfg, const char *path);

/*===========================================================================
 * Utilities
 *===========================================================================*/

/**
 * @brief Get density zone for track
 */
uft_nib_zone_t uft_nib_get_zone(int track);

/**
 * @brief Get sectors per track for zone
 */
int uft_nib_sectors_for_zone(uft_nib_zone_t zone);

/**
 * @brief Get expected track length (bytes)
 */
int uft_nib_track_length(uft_nib_zone_t zone);

/**
 * @brief Get last error message
 */
const char* uft_nib_get_error(const uft_nib_config_t *cfg);

/**
 * @brief Free track data
 */
void uft_nib_track_free(uft_nib_track_t *track);

#ifdef __cplusplus
}
#endif

#endif /* UFT_NIBTOOLS_H */
