/**
 * @file uft_otdr_encoding_boost.h
 * @brief OTDR-powered encoding detection boost for format detection pipeline
 *
 * Uses OTDR histogram peak analysis, sync-pattern correlation, and multi-scale
 * SNR profiling to detect disk encoding type (MFM/FM/GCR) with high confidence.
 * Results feed into format detection as a bonus signal.
 *
 * @version 1.0.0
 * @date 2026-04-08
 */

#ifndef UFT_OTDR_ENCODING_BOOST_H
#define UFT_OTDR_ENCODING_BOOST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uft/analysis/floppy_otdr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_ENC_BOOST_HIST_BINS     256     /**< Histogram bins (100ns each) */
#define UFT_ENC_BOOST_MAX_PEAKS     8       /**< Maximum histogram peaks */
#define UFT_ENC_BOOST_MIN_FLUX      1000    /**< Minimum flux transitions */

/* ═══════════════════════════════════════════════════════════════════════════
 * Data Types
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Encoding detection result from OTDR analysis */
typedef struct {
    otdr_encoding_t encoding;           /**< Detected encoding type */
    float           confidence;         /**< Overall confidence 0..1 */
    float           peak_separation;    /**< Peak separation ratio (1.5=MFM) */
    float           sync_quality;       /**< Sync pattern match quality 0..1 */
    uint32_t        sync_count;         /**< Number of sync patterns found */
    uint32_t        data_rate_bps;      /**< Estimated data rate in bps */
    uint32_t        cell_ns;            /**< Estimated bitcell time in ns */
    bool            is_high_density;    /**< HD vs DD */
    char            description[64];    /**< Human-readable description */
} uft_encoding_boost_result_t;

/** Internal histogram peak descriptor */
typedef struct {
    uint32_t bin;           /**< Peak bin index */
    uint32_t ns;            /**< Peak position in nanoseconds */
    uint32_t count;         /**< Peak height (sample count) */
    float    prominence;    /**< Peak prominence (relative height) */
} uft_enc_peak_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Run OTDR-powered encoding detection on raw flux data.
 *
 * Builds timing histogram, finds peaks, analyzes separation ratios,
 * correlates sync patterns, and scores candidate encodings.
 *
 * @param flux_ns   Raw flux intervals in nanoseconds
 * @param count     Number of flux intervals (must be >= UFT_ENC_BOOST_MIN_FLUX)
 * @param result    Output result (caller allocates)
 * @return 0 on success, -1 on invalid input, -2 on insufficient data
 */
int uft_otdr_detect_encoding(
    const uint32_t *flux_ns,
    uint32_t count,
    uft_encoding_boost_result_t *result);

/**
 * @brief Apply OTDR encoding result as score adjustment to format detection.
 *
 * If OTDR encoding matches the format profile's expected encoding:
 *   score += otdr_confidence * 0.10
 * If OTDR encoding disagrees and OTDR confidence > 0.7:
 *   score -= otdr_confidence * 0.20
 * Result is clamped to [0.0, 1.0].
 *
 * @param format_encoding   Encoding expected by the format profile
 * @param otdr_result       OTDR detection result
 * @param base_score        Current format detection score (0..1)
 * @return Adjusted score (0..1)
 */
float uft_encoding_boost_score(
    otdr_encoding_t format_encoding,
    const uft_encoding_boost_result_t *otdr_result,
    float base_score);

/**
 * @brief Get human-readable name for an encoding type.
 * @param enc  Encoding type
 * @return Static string (never NULL)
 */
const char *uft_otdr_encoding_name(otdr_encoding_t enc);

#ifdef __cplusplus
}
#endif

#endif /* UFT_OTDR_ENCODING_BOOST_H */
