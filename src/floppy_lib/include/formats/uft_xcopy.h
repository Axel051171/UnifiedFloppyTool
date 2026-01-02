/**
 * @file uft_xcopy.h
 * @brief XCopy-style disk duplication and nibble copy support
 * 
 * This module provides tools for:
 * - Full disk duplication with protection preservation
 * - Nibble-level (raw encoded) copying
 * - Track timing preservation
 * - Copy protection bypass/preservation options
 * 
 * Inspired by classic disk copy tools:
 * - X-Copy (Amiga)
 * - Disk-2-Disk (Commodore)
 * - Locksmith/EDD (Apple II)
 * - CopyIIPC (IBM PC)
 */

#ifndef UFT_XCOPY_H
#define UFT_XCOPY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Copy Modes
 * ============================================================================ */

/**
 * @brief Copy mode selection
 */
typedef enum {
    UFT_COPY_SECTOR,            /**< Standard sector copy */
    UFT_COPY_TRACK,             /**< Raw track copy */
    UFT_COPY_NIBBLE,            /**< Nibble-level copy (raw encoded) */
    UFT_COPY_FLUX,              /**< Flux-level copy */
    UFT_COPY_ANALYZE,           /**< Analyze and copy (auto-detect) */
} uft_copy_mode_t;

/**
 * @brief Copy options
 */
typedef struct {
    uft_copy_mode_t mode;           /**< Copy mode */
    
    /* Track selection */
    uint8_t start_track;            /**< First track to copy */
    uint8_t end_track;              /**< Last track to copy */
    bool both_sides;                /**< Copy both sides */
    bool half_tracks;               /**< Include half-tracks */
    
    /* Timing options */
    bool preserve_timing;           /**< Preserve track timing */
    bool preserve_gaps;             /**< Preserve gap lengths */
    bool preserve_sync;             /**< Preserve sync patterns */
    
    /* Protection handling */
    bool detect_protection;         /**< Detect copy protection */
    bool preserve_protection;       /**< Preserve protection schemes */
    bool strip_protection;          /**< Remove protection (if possible) */
    
    /* Error handling */
    uint8_t max_retries;            /**< Maximum read retries */
    bool ignore_errors;             /**< Continue on read errors */
    bool verify_copy;               /**< Verify after writing */
    
    /* Multi-revolution reading */
    uint8_t revolutions;            /**< Revolutions to read (1-10) */
    bool merge_revolutions;         /**< Merge multiple reads */
    
    /* Progress callback */
    void (*progress_cb)(uint8_t track, uint8_t side, 
                        const char *status, void *user);
    void *progress_user;
} uft_copy_options_t;

/**
 * @brief Initialize default copy options
 */
void uft_copy_options_init(uft_copy_options_t *opts);

/* ============================================================================
 * Copy Results
 * ============================================================================ */

/**
 * @brief Per-track copy result
 */
typedef struct {
    uint8_t track;                  /**< Track number */
    uint8_t side;                   /**< Side number */
    
    bool read_ok;                   /**< Read successful */
    bool write_ok;                  /**< Write successful */
    bool verify_ok;                 /**< Verification passed */
    
    uint8_t retries;                /**< Retries needed */
    uint8_t errors;                 /**< Error count */
    
    size_t raw_size;                /**< Raw track size */
    uint8_t sectors_found;          /**< Sectors detected */
    
    bool has_protection;            /**< Protection detected */
    uint32_t protection_flags;      /**< Protection type flags */
    
    char status[64];                /**< Status message */
} uft_track_copy_result_t;

/**
 * @brief Complete copy operation result
 */
typedef struct {
    uint32_t tracks_total;          /**< Total tracks attempted */
    uint32_t tracks_ok;             /**< Tracks copied successfully */
    uint32_t tracks_errors;         /**< Tracks with errors */
    
    uint32_t sectors_total;         /**< Total sectors */
    uint32_t sectors_ok;            /**< Sectors copied OK */
    uint32_t sectors_bad;           /**< Bad sectors */
    
    bool protection_detected;       /**< Any protection found */
    uint32_t protection_types;      /**< Protection type flags */
    
    double elapsed_seconds;         /**< Time taken */
    
    uft_track_copy_result_t *track_results;
    size_t track_result_count;
} uft_copy_result_t;

/* ============================================================================
 * Copy Operations
 * ============================================================================ */

/**
 * @brief Copy disk to disk
 * 
 * Copies from source drive to destination drive.
 * 
 * @param src_drive     Source drive identifier
 * @param dst_drive     Destination drive identifier  
 * @param opts          Copy options
 * @param result        Output: copy results
 * @return 0 on success
 */
int uft_copy_disk(
    const char *src_drive,
    const char *dst_drive,
    const uft_copy_options_t *opts,
    uft_copy_result_t *result
);

/**
 * @brief Copy disk to image file
 * 
 * @param src_drive     Source drive identifier
 * @param dst_image     Destination image path
 * @param image_format  Image format to create
 * @param opts          Copy options
 * @param result        Output: copy results
 * @return 0 on success
 */
int uft_copy_to_image(
    const char *src_drive,
    const char *dst_image,
    int image_format,
    const uft_copy_options_t *opts,
    uft_copy_result_t *result
);

/**
 * @brief Copy image to disk
 * 
 * @param src_image     Source image path
 * @param dst_drive     Destination drive identifier
 * @param opts          Copy options
 * @param result        Output: copy results
 * @return 0 on success
 */
int uft_copy_from_image(
    const char *src_image,
    const char *dst_drive,
    const uft_copy_options_t *opts,
    uft_copy_result_t *result
);

/**
 * @brief Convert image format
 * 
 * @param src_image     Source image path
 * @param dst_image     Destination image path
 * @param dst_format    Destination format
 * @param opts          Copy options
 * @param result        Output: copy results
 * @return 0 on success
 */
int uft_copy_image(
    const char *src_image,
    const char *dst_image,
    int dst_format,
    const uft_copy_options_t *opts,
    uft_copy_result_t *result
);

/**
 * @brief Free copy result
 */
void uft_copy_result_free(uft_copy_result_t *result);

/* ============================================================================
 * Nibble Operations
 * ============================================================================ */

/**
 * @brief Raw nibble track data
 */
typedef struct {
    uint8_t track;                  /**< Track number */
    uint8_t side;                   /**< Side number */
    
    uint8_t *data;                  /**< Raw encoded data */
    size_t data_len;                /**< Data length */
    
    uint32_t *timing;               /**< Timing data (optional) */
    size_t timing_len;              /**< Timing array length */
    
    uint8_t encoding;               /**< 0=MFM, 1=GCR, 2=FM */
    uint16_t bitrate;               /**< Bit rate in kbps */
    
    bool has_weak_bits;             /**< Weak bits detected */
    uint8_t *weak_mask;             /**< Weak bit mask */
} uft_nibble_track_t;

/**
 * @brief Read track as raw nibbles
 * 
 * @param drive         Drive identifier
 * @param track         Track number
 * @param side          Side number
 * @param revolutions   Revolutions to read
 * @param output        Output: nibble track data
 * @return 0 on success
 */
int uft_nibble_read_track(
    const char *drive,
    uint8_t track,
    uint8_t side,
    uint8_t revolutions,
    uft_nibble_track_t *output
);

/**
 * @brief Write raw nibbles to track
 * 
 * @param drive         Drive identifier
 * @param nibble        Nibble track data
 * @param verify        Verify after write
 * @return 0 on success
 */
int uft_nibble_write_track(
    const char *drive,
    const uft_nibble_track_t *nibble,
    bool verify
);

/**
 * @brief Free nibble track data
 */
void uft_nibble_track_free(uft_nibble_track_t *nibble);

/**
 * @brief Analyze nibble data
 * 
 * Analyzes raw nibble data to detect:
 * - Encoding type
 * - Sector layout
 * - Protection schemes
 * - Weak bit locations
 * 
 * @param nibble        Nibble track data
 * @param analysis      Output buffer for analysis text
 * @param analysis_len  Buffer size
 * @return Length of analysis text
 */
size_t uft_nibble_analyze(
    const uft_nibble_track_t *nibble,
    char *analysis,
    size_t analysis_len
);

/* ============================================================================
 * Track Synchronization
 * ============================================================================ */

/**
 * @brief Synchronization point in track
 */
typedef struct {
    uint32_t offset;                /**< Byte offset */
    uint32_t bit_offset;            /**< Bit offset (for nibbles) */
    uint8_t type;                   /**< Sync type (sector/gap/etc) */
    uint8_t sector;                 /**< Sector number (if applicable) */
} uft_sync_point_t;

/**
 * @brief Find sync points in nibble data
 * 
 * @param nibble        Nibble track data
 * @param points        Output: array of sync points
 * @param max_points    Maximum points to find
 * @return Number of sync points found
 */
size_t uft_find_sync_points(
    const uft_nibble_track_t *nibble,
    uft_sync_point_t *points,
    size_t max_points
);

/**
 * @brief Align two nibble tracks
 * 
 * Finds the alignment offset to match two track readings.
 * 
 * @param track1        First track
 * @param track2        Second track
 * @param offset        Output: alignment offset
 * @return 0 on success
 */
int uft_align_tracks(
    const uft_nibble_track_t *track1,
    const uft_nibble_track_t *track2,
    int32_t *offset
);

/* ============================================================================
 * Timing Preservation
 * ============================================================================ */

/**
 * @brief Track timing information
 */
typedef struct {
    uint32_t rotation_us;           /**< Rotation time in microseconds */
    uint16_t bitcell_ns;            /**< Nominal bit cell in nanoseconds */
    
    float *density_map;             /**< Bit density across track */
    size_t density_len;             /**< Density map length */
    
    uint16_t *speed_zones;          /**< Speed zone changes */
    size_t zone_count;              /**< Number of zones */
} uft_track_timing_t;

/**
 * @brief Measure track timing
 * 
 * @param drive         Drive identifier
 * @param track         Track number
 * @param timing        Output: timing information
 * @return 0 on success
 */
int uft_measure_timing(
    const char *drive,
    uint8_t track,
    uft_track_timing_t *timing
);

/**
 * @brief Apply timing to nibble data
 * 
 * @param nibble        Nibble track to modify
 * @param timing        Timing to apply
 * @return 0 on success
 */
int uft_apply_timing(
    uft_nibble_track_t *nibble,
    const uft_track_timing_t *timing
);

/**
 * @brief Free timing data
 */
void uft_timing_free(uft_track_timing_t *timing);

/* ============================================================================
 * Disk Comparison
 * ============================================================================ */

/**
 * @brief Compare two disks or images
 * 
 * @param disk1         First disk/image path
 * @param disk2         Second disk/image path
 * @param mode          Comparison mode
 * @param report        Output buffer for report
 * @param report_len    Report buffer size
 * @return Number of differences found
 */
int uft_compare_disks(
    const char *disk1,
    const char *disk2,
    uft_copy_mode_t mode,
    char *report,
    size_t report_len
);

/**
 * @brief Verify disk against image
 * 
 * @param drive         Drive to verify
 * @param image         Image to compare against
 * @param opts          Comparison options
 * @param result        Output: verification result
 * @return 0 if identical
 */
int uft_verify_disk(
    const char *drive,
    const char *image,
    const uft_copy_options_t *opts,
    uft_copy_result_t *result
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_XCOPY_H */
