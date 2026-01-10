/**
 * @file uft_xcopy_algo.h
 * @brief XCopy-Style Algorithms for Track Analysis
 * @version 1.0.0
 * 
 * Algorithms inspired by XCopy Pro (Amiga) and ManageDsk:
 * - Track length measurement (getracklen)
 * - Multi-revolution reading (NibbleRead)
 * - Per-drive calibration (mestrack)
 * - Timed sector scanning (FD_TIMED_SCAN_RESULT)
 * 
 * "Bei uns geht kein Bit verloren"
 */

#ifndef UFT_XCOPY_ALGO_H
#define UFT_XCOPY_ALGO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uft/uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Copy Modes (XCopy Pro compatible)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_COPY_DOS = 0,        /**< DOS/Sector level copy (fast) */
    UFT_COPY_BAM = 1,        /**< BAM-based (only allocated blocks) */
    UFT_COPY_DOSPLUS = 2,    /**< DOS with extensions */
    UFT_COPY_NIBBLE = 3,     /**< Nibble/bit-level copy */
    UFT_COPY_OPTIMIZE = 4,   /**< Optimized nibble */
    UFT_COPY_FORMAT = 5,     /**< Format track */
    UFT_COPY_QFORMAT = 6,    /**< Quick format */
    UFT_COPY_FLUX = 7,       /**< Full flux capture */
} uft_copy_mode_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Track Measurement (from XCopy Pro getracklen)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Track length measurement result
 */
typedef struct {
    uint32_t length_bytes;       /**< Measured track length in bytes */
    uint32_t length_bits;        /**< Measured track length in bits */
    uint32_t first_data_offset;  /**< Offset to first non-zero data */
    uint32_t last_data_offset;   /**< Offset to last non-zero data */
    uint32_t sync_count;         /**< Number of sync patterns found */
    uint32_t gap_total;          /**< Total gap bytes */
    float    density_ratio;      /**< Data density ratio */
    bool     valid;              /**< Measurement valid */
} uft_track_measure_t;

/**
 * @brief Measure actual track length (XCopy Pro algorithm)
 * 
 * Finds the last non-zero word in the buffer and calculates
 * the actual track length, not assuming standard sizes.
 * 
 * @param raw_data Raw track data (MFM encoded)
 * @param buffer_size Size of buffer
 * @param out Output measurement
 * @return UFT_SUCCESS or error
 */
uft_error_t uft_track_measure_length(const uint8_t *raw_data,
                                     size_t buffer_size,
                                     uft_track_measure_t *out);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Sync Pattern Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Sync pattern location
 */
typedef struct {
    uint32_t offset;         /**< Byte offset in track */
    uint16_t pattern;        /**< Sync pattern found */
    uint8_t  type;           /**< Sync type (MFM/GCR/etc) */
    uint8_t  confidence;     /**< Detection confidence (0-100) */
} uft_sync_pos_t;

/**
 * @brief Find all sync positions in track
 * @param raw_data Raw track data
 * @param len Data length
 * @param sync_pattern Sync pattern to find (e.g., 0x4489 for MFM)
 * @param positions Output array (caller allocated)
 * @param max_positions Array size
 * @param found_count Output: number found
 * @return UFT_SUCCESS or error
 */
uft_error_t uft_track_find_sync_positions(const uint8_t *raw_data,
                                          size_t len,
                                          uint16_t sync_pattern,
                                          uft_sync_pos_t *positions,
                                          size_t max_positions,
                                          size_t *found_count);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Multi-Revolution Reading (XCopy Pro NibbleRead)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Multi-revolution capture result
 */
typedef struct {
    uint8_t  *revolutions[16];   /**< Data for each revolution */
    size_t   rev_lengths[16];    /**< Length of each revolution */
    size_t   num_revolutions;    /**< Number of revolutions captured */
    uint32_t index_positions[16];/**< Index pulse positions */
    float    rpm_measured[16];   /**< Measured RPM per revolution */
    float    rpm_average;        /**< Average RPM */
    float    rpm_jitter;         /**< RPM variation */
} uft_multirev_t;

/**
 * @brief Read multiple revolutions of a track
 * 
 * XCopy Pro style: captures 2+ revolutions to ensure complete
 * track data and enable confidence-based merging.
 * 
 * @param buffer Raw flux/MFM data buffer (pre-captured)
 * @param buffer_size Buffer size
 * @param expected_rev_len Expected bytes per revolution
 * @param out Output multi-revolution data
 * @return UFT_SUCCESS or error
 */
uft_error_t uft_track_split_revolutions(const uint8_t *buffer,
                                        size_t buffer_size,
                                        size_t expected_rev_len,
                                        uft_multirev_t *out);

/**
 * @brief Align multiple revolutions for comparison
 * @param multirev Multi-revolution data
 * @return UFT_SUCCESS or error
 */
uft_error_t uft_track_align_revolutions(uft_multirev_t *multirev);

/**
 * @brief Merge revolutions with confidence weighting
 * @param multirev Multi-revolution input
 * @param output Output merged track
 * @param output_size Output buffer size
 * @param merged_len Output: merged length
 * @return UFT_SUCCESS or error
 */
uft_error_t uft_track_merge_revolutions(const uft_multirev_t *multirev,
                                        uint8_t *output,
                                        size_t output_size,
                                        size_t *merged_len);

/**
 * @brief Free multi-revolution data
 */
void uft_multirev_free(uft_multirev_t *multirev);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Timed Sector Scanning (fdrawcmd.sys FD_TIMED_SCAN_RESULT)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Sector timing information
 */
typedef struct {
    uint8_t  cyl;            /**< Cylinder (C) */
    uint8_t  head;           /**< Head (H) */
    uint8_t  sector;         /**< Sector (R) */
    uint8_t  size_code;      /**< Size code (N): actual = 128 << N */
    uint32_t rel_time_us;    /**< Time relative to index (µs) */
    uint32_t header_time_us; /**< Header field duration (µs) */
    uint32_t data_time_us;   /**< Data field duration (µs) */
    uint32_t gap_after_us;   /**< Gap after sector (µs) */
    uint8_t  st1;            /**< FDC Status Register 1 */
    uint8_t  st2;            /**< FDC Status Register 2 */
    bool     valid;          /**< Sector valid */
    bool     deleted;        /**< Deleted data mark */
    bool     crc_error;      /**< CRC error */
} uft_sector_timing_t;

/**
 * @brief Track timing analysis result
 */
typedef struct {
    uft_sector_timing_t sectors[64]; /**< Sector timings */
    size_t   sector_count;          /**< Number of sectors found */
    uint32_t track_time_us;         /**< Total track time (µs) */
    uint32_t index_to_first_us;     /**< Time from index to first sector */
    uint8_t  first_seen;            /**< First sector detected */
    float    rpm_calculated;        /**< RPM from track time */
    bool     consistent_timing;     /**< Timing consistent? */
    bool     protection_detected;   /**< Copy protection detected */
    char     protection_type[32];   /**< Protection type if detected */
} uft_track_timing_t;

/**
 * @brief Analyze track with sector timing
 * 
 * Uses fdrawcmd.sys style timed scanning to get precise
 * sector positions and detect copy protection schemes.
 * 
 * @param raw_data Raw track data
 * @param len Data length
 * @param encoding Encoding (MFM/FM/GCR)
 * @param out Output timing analysis
 * @return UFT_SUCCESS or error
 */
uft_error_t uft_track_analyze_timing(const uint8_t *raw_data,
                                     size_t len,
                                     uint8_t encoding,
                                     uft_track_timing_t *out);

/**
 * @brief Detect copy protection from timing
 * @param timing Track timing data
 * @param protection_name Output: protection name if found
 * @param name_size Buffer size
 * @return true if protection detected
 */
bool uft_protection_detect_from_timing(const uft_track_timing_t *timing,
                                       char *protection_name,
                                       size_t name_size);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Per-Drive Calibration (XCopy Pro mestrack/drilen[])
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Drive calibration data
 */
typedef struct {
    uint32_t track_lengths[4];   /**< Measured track length per drive (0-3) */
    float    rpm_measured[4];    /**< Measured RPM per drive */
    int32_t  offset_bytes[4];    /**< Offset adjustment per drive */
    bool     calibrated[4];      /**< Drive calibrated? */
    uint8_t  drive_type[4];      /**< Drive type (3.5", 5.25", etc) */
} uft_drive_calibration_t;

/**
 * @brief Initialize calibration data
 */
void uft_drive_calibration_init(uft_drive_calibration_t *cal);

/**
 * @brief Calibrate a drive by measuring track length
 * 
 * XCopy Pro stores drilen[] per drive. This measures the actual
 * track length a drive produces and stores for later use.
 * 
 * @param cal Calibration data
 * @param drive Drive number (0-3)
 * @param track_data Sample track data
 * @param track_len Track data length
 * @return UFT_SUCCESS or error
 */
uft_error_t uft_drive_calibrate(uft_drive_calibration_t *cal,
                                int drive,
                                const uint8_t *track_data,
                                size_t track_len);

/**
 * @brief Get optimal write length for source→target copy
 * 
 * XCopy Pro: MIN(SourceLen, TargetLen) + offset
 * 
 * @param cal Calibration data
 * @param source_drive Source drive
 * @param target_drive Target drive
 * @param offset Additional offset
 * @return Optimal write length in bytes
 */
uint32_t uft_drive_get_write_length(const uft_drive_calibration_t *cal,
                                    int source_drive,
                                    int target_drive,
                                    int32_t offset);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Copy Mode Selection
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Recommend copy mode based on format analysis
 * @param format_name Format name (e.g., "ADF", "D64", "XDF")
 * @param has_protection Protection detected
 * @param timing Track timing (optional)
 * @return Recommended copy mode
 */
uft_copy_mode_t uft_recommend_copy_mode(const char *format_name,
                                        bool has_protection,
                                        const uft_track_timing_t *timing);

/**
 * @brief Get copy mode name
 */
const char *uft_copy_mode_name(uft_copy_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* UFT_XCOPY_ALGO_H */
