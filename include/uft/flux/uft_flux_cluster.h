/**
 * @file uft_flux_cluster.h
 * @brief Flux Pulse Clustering and Pattern Search
 * 
 * Based on flux-analyze by various contributors
 * 
 * Implements:
 * - K-median clustering for flux band detection
 * - Ordinal pattern search for sync detection
 * - Band interval classification
 */

#ifndef UFT_FLUX_CLUSTER_H
#define UFT_FLUX_CLUSTER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Number of bands for MFM (1T, 2T, 3T delays) */
#define UFT_FLUX_NUM_BANDS      3

/** Maximum run length in MFM (RNNNR = 3 zeros between reversals) */
#define UFT_MFM_MAX_RUN_LENGTH  3

/** Maximum run length in FM (RNR = 1 zero between reversals) */
#define UFT_FM_MAX_RUN_LENGTH   2

/*===========================================================================
 * Structures
 *===========================================================================*/

/**
 * @brief Band interval definition
 */
typedef struct {
    int32_t min;            /**< Minimum value in band */
    int32_t max;            /**< Maximum value in band */
} uft_flux_interval_t;

/**
 * @brief Clustering result
 */
typedef struct {
    double centers[UFT_FLUX_NUM_BANDS];         /**< Cluster centers */
    uft_flux_interval_t intervals[UFT_FLUX_NUM_BANDS]; /**< Band intervals */
    double residual_sum;    /**< Sum of absolute residuals */
    bool valid;             /**< true if intervals don't overlap */
} uft_flux_clusters_t;

/**
 * @brief Pattern match result
 */
typedef struct {
    size_t position;        /**< Start position in flux array */
    uft_flux_clusters_t clustering;  /**< Local clustering from match */
    double confidence;      /**< Match confidence (0-1) */
} uft_flux_match_t;

/**
 * @brief Flux stream analysis context
 */
typedef struct {
    int32_t *flux_times;    /**< Raw flux timing data */
    size_t flux_count;      /**< Number of flux transitions */
    
    /* Derived data */
    uint8_t *assignments;   /**< Band assignments (0, 1, 2) */
    int8_t *residuals;      /**< Signed residuals from cluster centers */
    
    /* Global clustering */
    uft_flux_clusters_t global_clusters;
    
    /* Preamble matches */
    uft_flux_match_t *matches;
    size_t match_count;
    size_t match_capacity;
} uft_flux_ctx_t;

/*===========================================================================
 * Ordinal Pattern Functions
 *===========================================================================*/

/**
 * @brief Compute ordinal pattern from flux delays
 * 
 * Creates a binary pattern where 1 indicates an increase from the
 * previous value and 0 indicates a decrease or equal.
 * 
 * @param flux_times Flux timing array
 * @param count Number of values
 * @param pattern Output pattern (count-1 bytes)
 */
void uft_flux_ordinal_pattern(const int32_t *flux_times, size_t count,
                               uint8_t *pattern);

/**
 * @brief Search for ordinal pattern
 * 
 * Finds positions where the ordinal pattern matches, regardless of
 * actual timing values. This allows finding sync patterns without
 * knowing the clock rate.
 * 
 * @param flux_times Haystack flux times
 * @param flux_count Number of flux times
 * @param needle_assignments Expected band assignments for pattern
 * @param needle_len Pattern length
 * @param matches Output array of match positions
 * @param max_matches Maximum matches to return
 * @return Number of matches found
 */
size_t uft_flux_ordinal_search(const int32_t *flux_times, size_t flux_count,
                                const uint8_t *needle_assignments, size_t needle_len,
                                size_t *matches, size_t max_matches);

/*===========================================================================
 * Band Classification Functions
 *===========================================================================*/

/**
 * @brief Classify flux points into bands based on pattern match
 * 
 * Given a known pattern match position and expected assignments,
 * determines the actual flux values belonging to each band.
 * 
 * @param flux_times Flux timing array
 * @param match_pos Start position of match
 * @param assignments Expected band assignments
 * @param pattern_len Pattern length
 * @param intervals Output band intervals
 * @return true if classification is valid (non-overlapping intervals)
 */
bool uft_flux_classify_bands(const int32_t *flux_times, size_t match_pos,
                              const uint8_t *assignments, size_t pattern_len,
                              uft_flux_interval_t intervals[UFT_FLUX_NUM_BANDS]);

/**
 * @brief Check if band intervals are valid (non-overlapping)
 */
static inline bool uft_flux_intervals_valid(const uft_flux_interval_t intervals[UFT_FLUX_NUM_BANDS])
{
    for (int i = 0; i < UFT_FLUX_NUM_BANDS - 1; i++) {
        if (intervals[i].max >= intervals[i + 1].min) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Convert band intervals to cluster centers
 * 
 * Computes optimal cluster centers from intervals such that
 * assignment by nearest-neighbor will correctly separate the bands.
 */
bool uft_flux_intervals_to_centers(const uft_flux_interval_t intervals[UFT_FLUX_NUM_BANDS],
                                    double centers[UFT_FLUX_NUM_BANDS]);

/*===========================================================================
 * Clustering Functions
 *===========================================================================*/

/**
 * @brief K-median clustering for flux data
 * 
 * Finds optimal cluster centers to minimize sum of absolute deviations.
 * Uses weighted median for robustness against outliers.
 * 
 * @param flux_times Flux timing array
 * @param count Number of values
 * @param k Number of clusters (typically 3 for MFM)
 * @param centers Output cluster centers
 * @return Sum of absolute residuals
 */
double uft_flux_k_median(const int32_t *flux_times, size_t count,
                          int k, double *centers);

/**
 * @brief K-center clustering with radius constraint
 * 
 * Finds cluster centers such that every point is within
 * specified radius of its nearest center.
 * 
 * @param flux_times Flux timing array
 * @param count Number of values
 * @param k Number of clusters
 * @param max_radius Maximum allowed radius
 * @param centers Output cluster centers
 * @return Actual maximum radius achieved, or -1 if impossible
 */
double uft_flux_k_center(const int32_t *flux_times, size_t count,
                          int k, double max_radius, double *centers);

/**
 * @brief Assign flux values to clusters
 * 
 * @param flux_times Flux timing array
 * @param count Number of values
 * @param centers Cluster centers
 * @param k Number of clusters
 * @param assignments Output assignments (0 to k-1)
 * @param residuals Output signed residuals (optional, can be NULL)
 */
void uft_flux_assign_clusters(const int32_t *flux_times, size_t count,
                               const double *centers, int k,
                               uint8_t *assignments, int16_t *residuals);

/*===========================================================================
 * Preamble Detection
 *===========================================================================*/

/** 
 * IBM MFM A1 sync byte pattern (with missing clock)
 * Binary: 10100001
 * MFM: 0100 0100 1000 1001
 * Assignments: 1,2,1,2,1,0,2,1,2,1,0,2,1,2,1,0 (for A1A1A1)
 */
extern const uint8_t uft_mfm_a1_assignments[];
extern const size_t uft_mfm_a1_len;

/**
 * IBM MFM C2 sync byte pattern (for IAM)
 * Binary: 11000010
 * MFM: 0101 0010 0010 0100
 */
extern const uint8_t uft_mfm_c2_assignments[];
extern const size_t uft_mfm_c2_len;

/**
 * @brief Find A1A1A1 preamble positions
 * 
 * Searches for the IBM MFM sync pattern using ordinal search
 * followed by interval validation.
 * 
 * @param flux_times Flux timing array
 * @param count Number of values
 * @param matches Output match array
 * @param max_matches Maximum matches
 * @return Number of valid matches found
 */
size_t uft_flux_find_a1_preambles(const int32_t *flux_times, size_t count,
                                   uft_flux_match_t *matches, size_t max_matches);

/**
 * @brief Find C2C2C2 preamble positions (for IAM)
 */
size_t uft_flux_find_c2_preambles(const int32_t *flux_times, size_t count,
                                   uft_flux_match_t *matches, size_t max_matches);

/*===========================================================================
 * Flux Stream Conversion
 *===========================================================================*/

/**
 * @brief Convert band assignments to binary flux stream
 * 
 * Band 0 (1T): produces "01" (no reversal, reversal)
 * Band 1 (2T): produces "001"
 * Band 2 (3T): produces "0001"
 * 
 * @param assignments Band assignment array
 * @param count Number of assignments
 * @param flux_stream Output flux stream (1 = reversal, 0 = no reversal)
 * @param positions Output: maps each flux bit to assignment index
 * @return Length of flux stream
 */
size_t uft_flux_assignments_to_stream(const uint8_t *assignments, size_t count,
                                       uint8_t *flux_stream, size_t *positions);

/**
 * @brief Convert binary flux stream to band assignments
 * 
 * The flux stream must start with a reversal (1).
 * 
 * @param flux_stream Input flux stream
 * @param len Stream length
 * @param max_run Maximum allowed run of zeros
 * @param assignments Output assignments
 * @return Number of assignments, or -1 on error
 */
int uft_flux_stream_to_assignments(const uint8_t *flux_stream, size_t len,
                                    int max_run, uint8_t *assignments);

/*===========================================================================
 * Context Management
 *===========================================================================*/

/**
 * @brief Initialize flux analysis context
 */
int uft_flux_ctx_init(uft_flux_ctx_t *ctx, const int32_t *flux_times, size_t count);

/**
 * @brief Free flux analysis context
 */
void uft_flux_ctx_free(uft_flux_ctx_t *ctx);

/**
 * @brief Perform global clustering on context
 */
int uft_flux_ctx_cluster(uft_flux_ctx_t *ctx);

/**
 * @brief Find all preambles in context
 */
int uft_flux_ctx_find_preambles(uft_flux_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLUX_CLUSTER_H */
