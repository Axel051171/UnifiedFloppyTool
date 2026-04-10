/**
 * @file uft_ml_protection.h
 * @brief ML-based Copy Protection Classifier
 *
 * Feature-based classifier that identifies floppy disk copy protection
 * schemes from statistical track features.  Uses cosine similarity
 * against hardcoded reference vectors for known schemes (V-MAX!,
 * RapidLok, CopyLock, Speedlock, etc.) and a decision threshold
 * for unknown-but-suspicious patterns.
 *
 * No external ML libraries required -- pure C with math.h.
 *
 * @author UFT Project
 * @license GPL-3.0
 */

#ifndef UFT_ML_PROTECTION_H
#define UFT_ML_PROTECTION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===================================================================
 * Constants
 * =================================================================== */

/** Number of features per track used for classification */
#define UFT_ML_PROT_FEATURES    8

/** Maximum number of candidate matches returned */
#define UFT_ML_PROT_MAX_CANDS   5

/** Minimum cosine similarity to consider a match */
#define UFT_ML_PROT_MATCH_MIN   0.70f

/** Threshold below which anomalous tracks are "unknown protection" */
#define UFT_ML_PROT_UNKNOWN_TH  0.50f

/* ===================================================================
 * Types
 * =================================================================== */

/** A single candidate protection scheme match. */
typedef struct {
    char     scheme_name[32];       /**< Human-readable scheme name */
    float    confidence;            /**< Match confidence 0.0 - 1.0 */
} uft_ml_prot_candidate_t;

/** Result of ML-based protection classification. */
typedef struct {
    uft_ml_prot_candidate_t candidates[UFT_ML_PROT_MAX_CANDS];
    int      count;                 /**< Number of valid candidates */
    float    unknown_probability;   /**< Probability of an unknown scheme */
    bool     is_protected;          /**< True if any protection detected */
    char     summary[256];          /**< Human-readable classification summary */
} uft_ml_prot_result_t;

/* ===================================================================
 * API Functions
 * =================================================================== */

/**
 * Classify disk protection using feature-based ML.
 *
 * Averages per-track feature vectors across all tracks, then computes
 * cosine similarity against known protection reference signatures.
 * Returns up to 5 candidate matches sorted by confidence.
 *
 * Feature vector layout per track (8 floats):
 *   [0] histogram_entropy     Shannon entropy of flux timing histogram
 *   [1] track_length_ratio    actual/nominal length (>1.02 = long track)
 *   [2] sync_pattern_score    custom sync frequency, normalised 0-1
 *   [3] bad_gcr_ratio         bad GCR bytes / total, normalised 0-1
 *   [4] duplicate_id_count    normalised 0-1
 *   [5] jitter_rms            RMS jitter, normalised 0-1
 *   [6] half_track_flag       0.0 or 1.0
 *   [7] custom_sync_flag      0.0 or 1.0
 *
 * @param track_features  Array of n_tracks x 8 feature vectors.
 * @param n_tracks        Number of tracks.
 * @param result          Output classification result (caller allocates).
 * @return 0 on success, -1 on error.
 */
int uft_ml_detect_protection(const float (*track_features)[UFT_ML_PROT_FEATURES],
                              int n_tracks,
                              uft_ml_prot_result_t *result);

/**
 * Extract the 8-element feature vector for a single track.
 *
 * This helper normalises raw track metrics into the feature space
 * expected by uft_ml_detect_protection().
 *
 * @param histogram         256-bin flux timing histogram (NULL-safe).
 * @param track_length_ratio Actual/nominal track length ratio.
 * @param sync_count        Number of custom sync patterns found.
 * @param bad_gcr_count     Number of invalid GCR bytes.
 * @param duplicate_ids     Number of duplicate sector IDs.
 * @param jitter_rms        RMS timing jitter in microseconds.
 * @param has_half_track    True if half-track data is present.
 * @param has_custom_sync   True if non-standard sync bytes found.
 * @param features_out      Output array of 8 floats (caller allocates).
 * @return 0 on success, -1 on error.
 */
int uft_ml_extract_features(const float *histogram,
                             float track_length_ratio,
                             int sync_count,
                             int bad_gcr_count,
                             int duplicate_ids,
                             float jitter_rms,
                             bool has_half_track,
                             bool has_custom_sync,
                             float features_out[UFT_ML_PROT_FEATURES]);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ML_PROTECTION_H */
