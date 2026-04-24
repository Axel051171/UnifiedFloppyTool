/**
 * @file uft_disk_quickscan.h
 * @brief CHECKDISK-style fast pre-capture triage (M2 T6).
 *
 * A "quick scan" sweeps every track looking for sync patterns and
 * counting CRC-ok / CRC-fail / no-sync results. It does NOT do a
 * full flux capture — goal is to triage a disk in 30s before
 * committing to a multi-minute forensic capture.
 *
 * This header defines the data model. HAL-integration (actually
 * reading the disk) is separate — use in combination with
 * uft_hal_quick_scan() when that lands in M3.
 *
 * Inspired by X-Copy's CHECKDISK routine. Part of MASTER_PLAN.md M2 T6.
 */
#ifndef UFT_DISK_QUICKSCAN_H
#define UFT_DISK_QUICKSCAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "uft/uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum tracks we report. 84 covers over-format tracks on 5.25"/3.5". */
#define UFT_QUICKSCAN_MAX_TRACKS   84u

/** Per-track health status for triage. */
typedef enum {
    UFT_QSTAT_UNKNOWN   = 0, /**< Not scanned yet */
    UFT_QSTAT_GOOD      = 1, /**< All sectors found, all CRCs OK */
    UFT_QSTAT_DEGRADED  = 2, /**< Some sectors with CRC errors or missing */
    UFT_QSTAT_UNREADABLE= 3, /**< No sync pulses found at all */
    UFT_QSTAT_BLANK     = 4  /**< Track visible but all-zero / unformatted */
} uft_qscan_track_status_t;

/** Per-track statistics from a quick-scan sweep. */
typedef struct {
    uint8_t                     track_num;    /**< 0..83 physical track */
    uint8_t                     side;         /**< 0 or 1 */
    uft_qscan_track_status_t    status;
    uint16_t                    syncs_found;
    uint16_t                    sectors_ok;
    uint16_t                    sectors_crc_fail;
    uint16_t                    sectors_missing;
    uint32_t                    elapsed_ms;
} uft_qscan_track_t;

/** Whole-disk quick-scan result. */
typedef struct {
    size_t                      track_count;
    uft_qscan_track_t           tracks[UFT_QUICKSCAN_MAX_TRACKS * 2]; /* both sides */

    /* Aggregate stats (populated by uft_qscan_summarise) */
    uint16_t                    total_good;
    uint16_t                    total_degraded;
    uint16_t                    total_unreadable;
    uint16_t                    total_blank;
    float                       health_percent; /**< 100 × good / total */
    uint32_t                    total_elapsed_ms;
} uft_qscan_result_t;


/**
 * Classify a track from raw sync/sector counters.
 *
 * Rules:
 *   - syncs_found == 0          → UNREADABLE
 *   - sectors_ok + crc_fail == 0 with syncs ≥ 1 → BLANK
 *   - crc_fail > 0 OR missing > 0 → DEGRADED
 *   - all OK                     → GOOD
 */
uft_qscan_track_status_t uft_qscan_classify_track(
    uint16_t syncs_found,
    uint16_t sectors_ok,
    uint16_t sectors_crc_fail,
    uint16_t sectors_missing);

/**
 * Analyse a pre-captured bitstream and fill in a track record.
 * Uses a lightweight sync+CRC scan — no full PLL or multi-rev fusion.
 * This lets callers who already have a bitstream run the triage
 * logic without going back to the hardware.
 *
 * @param bits          MFM bitstream (MSB-first)
 * @param bit_count     length in bits
 * @param sync_word     sync pattern to search (e.g. 0x4489 for Amiga MFM,
 *                      0x8914 for IBM PC MFM ID mark)
 * @param expected_secs expected sectors per track (e.g. 11 for Amiga DD,
 *                      18 for PC 1.44M MFM)
 * @param out           caller-allocated output
 *
 * @return UFT_OK on success, UFT_ERR_INVALID_ARG on bad inputs.
 */
uft_error_t uft_qscan_analyse_bitstream(
    const uint8_t *bits,
    size_t bit_count,
    uint16_t sync_word,
    uint16_t expected_secs,
    uft_qscan_track_t *out);

/**
 * Walk tracks[] and populate the aggregate counters + health_percent.
 * Safe to call multiple times.
 */
void uft_qscan_summarise(uft_qscan_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DISK_QUICKSCAN_H */
