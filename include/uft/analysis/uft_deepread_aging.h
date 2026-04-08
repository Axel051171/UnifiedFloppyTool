/**
 * @file uft_deepread_aging.h
 * @brief DeepRead Magnetic Aging Profile
 *
 * Analyzes the per-bitcell quality profile produced by the OTDR engine
 * to characterize magnetic media aging.  A linear regression of the
 * quality trace reveals the overall degradation slope, while residual
 * analysis pinpoints localized damage regions.  The combined metrics
 * are mapped to a five-level aging classification from PRISTINE to
 * DAMAGED.
 *
 * @author UFT Project
 * @license GPL-3.0
 */

#ifndef UFT_DEEPREAD_AGING_H
#define UFT_DEEPREAD_AGING_H

#include "uft/analysis/floppy_otdr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===================================================================
 * Types
 * =================================================================== */

/** Aging classification levels */
typedef enum {
    UFT_AGING_PRISTINE,     /**< Like-new media, no measurable degradation */
    UFT_AGING_MILD,         /**< Slight wear, fully readable */
    UFT_AGING_MODERATE,     /**< Noticeable aging, marginal regions present */
    UFT_AGING_SEVERE,       /**< Significant degradation, read errors likely */
    UFT_AGING_DAMAGED       /**< Extensive damage, data recovery required */
} uft_aging_class_t;

/** Aging analysis result for an entire disk */
typedef struct {
    uft_aging_class_t classification;   /**< Overall aging classification */
    float    slope;                     /**< Mean quality-profile slope across all tracks */
    float    r_squared;                 /**< Mean R-squared of per-track regressions */
    float    residual_max;              /**< Worst residual_max across all tracks */
    float    mean_snr_db;               /**< Mean SNR across all tracks (dB) */
    float    snr_gradient;              /**< Slope of per-track mean SNR over track number */
    uint32_t damage_regions;            /**< Number of tracks with residual_max > 10 dB */
} uft_aging_result_t;

/* ===================================================================
 * Functions
 * =================================================================== */

/**
 * Run a full DeepRead aging analysis on every track of a disk.
 *
 * @param disk   Analyzed disk (tracks must already have quality profiles).
 * @param result Output aging result.
 * @return 0 on success, -1 on invalid input.
 */
int uft_deepread_aging_analyze(const otdr_disk_t *disk,
                               uft_aging_result_t *result);

/**
 * Compute the linear regression of a single track's quality profile.
 *
 * @param track        Analyzed track with a valid quality_profile[].
 * @param slope        Output: regression slope (quality/bitcell).
 * @param r_squared    Output: coefficient of determination.
 * @param residual_max Output: maximum absolute residual.
 * @return 0 on success, -1 on invalid input.
 */
int uft_deepread_aging_track(const otdr_track_t *track,
                             float *slope, float *r_squared,
                             float *residual_max);

/**
 * Return a human-readable name for an aging classification.
 *
 * @param cls  Aging class.
 * @return Static string such as "Pristine" or "Damaged".
 */
const char *uft_aging_class_name(uft_aging_class_t cls);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DEEPREAD_AGING_H */
