/**
 * @file uft_recovery.h
 * @brief Floppy disk data recovery functions
 * 
 * Functions for recovering data from damaged or copy-protected disks:
 * - CRC error correction
 * - Weak bit recovery from multiple reads
 * - Sector reconstruction
 * - Track alignment recovery
 * - PLL re-synchronization
 */

#ifndef UFT_RECOVERY_H
#define UFT_RECOVERY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Recovery Status
 * ============================================================================ */

typedef enum {
    UFT_RECOVERY_OK = 0,
    UFT_RECOVERY_PARTIAL,      /**< Partial recovery */
    UFT_RECOVERY_CRC_FIXED,    /**< CRC error corrected */
    UFT_RECOVERY_FAILED,       /**< Recovery failed */
    UFT_RECOVERY_NO_DATA,      /**< No data to recover */
} uft_recovery_status_t;

/**
 * @brief Recovery statistics
 */
typedef struct {
    uint32_t sectors_read;      /**< Total sectors attempted */
    uint32_t sectors_ok;        /**< Sectors read without error */
    uint32_t sectors_recovered; /**< Sectors recovered from errors */
    uint32_t sectors_failed;    /**< Sectors that could not be recovered */
    uint32_t crc_errors_fixed;  /**< CRC errors corrected */
    uint32_t weak_bits_fixed;   /**< Weak bits resolved */
    uint32_t retries;           /**< Total retry attempts */
} uft_recovery_stats_t;

/* ============================================================================
 * CRC Error Correction
 * ============================================================================ */

/**
 * @brief Attempt to fix CRC error by bit-flipping
 * 
 * Tries flipping each bit in the data until CRC matches.
 * Only works for single-bit errors.
 * 
 * @param data          Data with CRC error
 * @param data_len      Data length
 * @param expected_crc  Expected CRC value
 * @param bit_flipped   Output: which bit was flipped (-1 if failed)
 * @return UFT_RECOVERY_CRC_FIXED on success
 */
uft_recovery_status_t uft_recovery_fix_crc_single(
    uint8_t *data,
    size_t data_len,
    uint16_t expected_crc,
    int *bit_flipped
);

/**
 * @brief Attempt to fix CRC error by flipping two bits
 * 
 * More expensive than single-bit, but handles burst errors.
 * 
 * @param data          Data with CRC error
 * @param data_len      Data length
 * @param expected_crc  Expected CRC value
 * @param bits_flipped  Output: array of bits flipped
 * @return UFT_RECOVERY_CRC_FIXED on success
 */
uft_recovery_status_t uft_recovery_fix_crc_double(
    uint8_t *data,
    size_t data_len,
    uint16_t expected_crc,
    int bits_flipped[2]
);

/**
 * @brief Attempt CRC correction using syndrome
 * 
 * Uses CRC syndrome to identify error location (if possible).
 * 
 * @param data          Data with CRC error
 * @param data_len      Data length
 * @param read_crc      CRC that was read from disk
 * @param calc_crc      CRC calculated from data
 * @return UFT_RECOVERY_CRC_FIXED on success
 */
uft_recovery_status_t uft_recovery_fix_crc_syndrome(
    uint8_t *data,
    size_t data_len,
    uint16_t read_crc,
    uint16_t calc_crc
);

/* ============================================================================
 * Weak Bit Recovery
 * ============================================================================ */

/**
 * @brief Resolve weak bits from multiple reads
 * 
 * Uses voting algorithm to determine most likely value.
 * 
 * @param reads         Array of read attempts
 * @param read_count    Number of reads
 * @param data_len      Length of each read
 * @param output        Output: resolved data
 * @param confidence    Output: confidence per byte (0-100)
 * @return UFT_RECOVERY_OK if all bytes resolved with high confidence
 */
uft_recovery_status_t uft_recovery_resolve_weak_bits(
    const uint8_t **reads,
    size_t read_count,
    size_t data_len,
    uint8_t *output,
    uint8_t *confidence
);

/**
 * @brief Identify weak bit positions
 * 
 * Finds positions where reads differ.
 * 
 * @param reads         Array of read attempts
 * @param read_count    Number of reads
 * @param data_len      Length of each read
 * @param weak_map      Output: bitmap of weak positions
 * @return Number of weak bit positions found
 */
size_t uft_recovery_find_weak_positions(
    const uint8_t **reads,
    size_t read_count,
    size_t data_len,
    uint8_t *weak_map
);

/* ============================================================================
 * Sector Recovery
 * ============================================================================ */

/**
 * @brief Sector recovery options
 */
typedef struct {
    uint8_t max_retries;        /**< Maximum read retries */
    bool try_crc_correction;    /**< Attempt CRC error correction */
    bool use_multiple_reads;    /**< Use multiple reads for weak bits */
    uint8_t min_confidence;     /**< Minimum confidence for success (0-100) */
    bool recover_partial;       /**< Return partial data on failure */
} uft_recovery_options_t;

/**
 * @brief Default recovery options
 */
extern const uft_recovery_options_t uft_recovery_defaults;

/**
 * @brief Aggressive recovery options
 */
extern const uft_recovery_options_t uft_recovery_aggressive;

/**
 * @brief Recovered sector data
 */
typedef struct {
    uint8_t *data;              /**< Sector data */
    size_t data_len;            /**< Data length */
    uft_recovery_status_t status; /**< Recovery status */
    uint8_t confidence;         /**< Overall confidence (0-100) */
    uint8_t *confidence_map;    /**< Per-byte confidence (optional) */
    uint32_t retries;           /**< Retries needed */
    bool crc_corrected;         /**< CRC error was corrected */
    int corrected_bit;          /**< Which bit was corrected (-1 if none) */
} uft_recovered_sector_t;

/**
 * @brief Free recovered sector data
 */
void uft_recovered_sector_free(uft_recovered_sector_t *sector);

/* ============================================================================
 * Track Recovery
 * ============================================================================ */

/**
 * @brief Re-synchronize PLL from raw track data
 * 
 * Attempts to find optimal PLL parameters for decoding.
 * 
 * @param track_data    Raw track data
 * @param track_len     Track data length
 * @param bit_cell      Initial bit cell estimate
 * @param best_clock    Output: optimal clock period
 * @param best_phase    Output: optimal phase
 * @return UFT_RECOVERY_OK on success
 */
uft_recovery_status_t uft_recovery_resync_pll(
    const uint8_t *track_data,
    size_t track_len,
    double bit_cell,
    double *best_clock,
    double *best_phase
);

/**
 * @brief Find sector boundaries in damaged track
 * 
 * Scans for sync patterns even with corrupted data.
 * 
 * @param track_data    Raw track data
 * @param track_len     Track data length
 * @param offsets       Output: array of sector start offsets
 * @param max_sectors   Maximum sectors to find
 * @return Number of sectors found
 */
size_t uft_recovery_find_sectors(
    const uint8_t *track_data,
    size_t track_len,
    uint32_t *offsets,
    size_t max_sectors
);

/**
 * @brief Reconstruct track from partial data
 * 
 * Uses multiple damaged reads to reconstruct complete track.
 * 
 * @param reads         Array of partial track reads
 * @param read_count    Number of reads
 * @param track_len     Expected track length
 * @param output        Output: reconstructed track
 * @param coverage      Output: percentage of track recovered
 * @return UFT_RECOVERY_OK if track fully reconstructed
 */
uft_recovery_status_t uft_recovery_reconstruct_track(
    const uint8_t **reads,
    size_t read_count,
    size_t track_len,
    uint8_t *output,
    uint8_t *coverage
);

/* ============================================================================
 * GCR-Specific Recovery
 * ============================================================================ */

/**
 * @brief Fix GCR decode errors
 * 
 * Attempts to correct invalid GCR codes.
 * 
 * @param gcr_data      GCR encoded data
 * @param gcr_len       Data length
 * @param decoded       Output: decoded data
 * @param decoded_len   Output: decoded length
 * @return UFT_RECOVERY_OK on success
 */
uft_recovery_status_t uft_recovery_fix_gcr(
    const uint8_t *gcr_data,
    size_t gcr_len,
    uint8_t *decoded,
    size_t *decoded_len
);

/* ============================================================================
 * Forensic Recovery
 * ============================================================================ */

/**
 * @brief Forensic recovery report
 */
typedef struct {
    char *report_text;          /**< Human-readable report */
    size_t report_len;          /**< Report length */
    uft_recovery_stats_t stats; /**< Recovery statistics */
    uint8_t *sector_status;     /**< Per-sector status */
    size_t sector_count;        /**< Number of sectors */
} uft_forensic_report_t;

/**
 * @brief Create forensic recovery report
 */
uft_forensic_report_t* uft_recovery_create_report(void);

/**
 * @brief Free forensic report
 */
void uft_recovery_free_report(uft_forensic_report_t *report);

/**
 * @brief Add entry to forensic report
 */
void uft_recovery_report_add(uft_forensic_report_t *report,
                              uint8_t track, uint8_t head, uint8_t sector,
                              uft_recovery_status_t status,
                              const char *message);

/**
 * @brief Generate final report text
 */
void uft_recovery_report_finalize(uft_forensic_report_t *report);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Calculate data integrity score
 * 
 * @param data      Data to check
 * @param len       Data length
 * @param expected  Expected checksum (or 0 for none)
 * @return Score 0-100 (100 = perfect)
 */
uint8_t uft_recovery_integrity_score(
    const uint8_t *data,
    size_t len,
    uint16_t expected
);

/**
 * @brief Estimate recoverability
 * 
 * Analyzes data to estimate how recoverable it is.
 * 
 * @param data      Damaged data
 * @param len       Data length
 * @return Percentage recoverable (0-100)
 */
uint8_t uft_recovery_estimate_recoverability(
    const uint8_t *data,
    size_t len
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_RECOVERY_H */
