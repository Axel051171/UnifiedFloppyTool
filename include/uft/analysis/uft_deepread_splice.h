/**
 * @file uft_deepread_splice.h
 * @brief DeepRead Write-Splice Detection
 *
 * Detects the write splice point on floppy disk tracks by analysing the
 * bitcell-level quality profile produced by the OTDR engine.  The write
 * splice is where the drive's write head started and finished a track
 * write -- it manifests as an abrupt phase/jitter discontinuity that is
 * consistent across all tracks written in one session.
 *
 * GEMESSEN MF-1431 (tests/test_deepread_naht.c; Spur aus
 * uft_mfm_encode_track(), Naht als Phasensprung, Jitter gewobbelt):
 *   - die LAGE stimmt bis 60 ns Jitter auf +-4 Zellen, ab 100 ns nicht;
 *   - `splice_stability` trennt Naht und Rauschen: dieselbe Naht auf zwei
 *     Umdrehungen 0-2,8 Zellen, ohne Naht 3 807-30 422 Zellen;
 *   - `detected` (Sprung > 3 dB, Schwelle ohne Quelle) meldet ohne jede
 *     Naht ab 60 ns Jitter 1 — KEIN Befund, das Panel zeigt es nicht;
 *   - das Modell ist eine Annahme, keine Aufnahme: an einer echten
 *     Diskette ist nichts davon gemessen (P3-630).
 *
 * @author UFT Project
 * @license GPL-3.0
 */

#ifndef UFT_DEEPREAD_SPLICE_H
#define UFT_DEEPREAD_SPLICE_H

#include "uft/analysis/floppy_otdr.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Types
 * ----------------------------------------------------------------------- */

/** Result of write-splice detection on a single track. */
typedef struct {
    uint32_t splice_bitcell;      /**< Position relative to track start */
    uint32_t splice_ns;           /**< Position in nanoseconds */
    float    splice_magnitude;    /**< Jitter jump in dB */
    float    splice_stability;    /**< Std-dev of the peak position across
                                       revolutions, in BITCELLS; each revolution
                                       measured like the track (MF-1431).
                                       0 with a single revolution. */
    int32_t  index_offset_ns;     /**< splice_ns - revolution_ns: the NEGATIVE
                                       distance to the NEXT index pulse */
    bool     detected;            /**< magnitude > 3 dB. Unsourced threshold,
                                       fires on jitter alone (MF-1431 W3) */
} uft_splice_result_t;

/* -----------------------------------------------------------------------
 * Functions
 * ----------------------------------------------------------------------- */

/**
 * Detect the write splice on a single track.
 *
 * Analyses the quality_profile to find the largest first-derivative
 * discontinuity, which corresponds to the write splice.  When multi-
 * revolution data is available the stability (standard deviation of
 * splice positions across revolutions) is also computed.
 *
 * @param track   Analysed OTDR track (must have quality_profile).
 * @param result  Output splice result.
 * @return 0 on success, -1 on invalid input.
 */
int uft_deepread_detect_splice(const otdr_track_t *track,
                               uft_splice_result_t *result);

/**
 * Build a disk-wide write-splice map and compute consistency.
 *
 * Calls uft_deepread_detect_splice() for every track in the disk and
 * then evaluates how consistently the splice falls at the same bitcell
 * position across all tracks (ratio of tracks within +/-50 bitcells of
 * the median splice position).
 *
 * @param disk        Analysed OTDR disk.
 * @param results     Output array, must hold at least disk->track_count entries.
 * @param consistency Output consistency ratio (0.0 .. 1.0).
 * @return 0 on success, -1 on invalid input.
 */
int uft_deepread_disk_splice_map(const otdr_disk_t *disk,
                                 uft_splice_result_t *results,
                                 float *consistency);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DEEPREAD_SPLICE_H */
