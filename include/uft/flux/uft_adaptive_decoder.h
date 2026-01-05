/**
 * @file uft_adaptive_decoder.h
 * @brief Adaptive Flux Decoder with Clustering
 * 
 * Based on FloppyAI custom_decoder.py
 * Ported from Python to C for UFT
 * 
 * Provides adaptive decoding using statistical analysis:
 * - K-means clustering for threshold detection
 * - Dynamic threshold adjustment based on flux statistics
 * - RPM normalization
 * - Manchester and RLL-like decoding
 * 
 * Key advantage: Works on unknown/non-standard formats by
 * learning thresholds from the actual flux data.
 */

#ifndef UFT_ADAPTIVE_DECODER_H
#define UFT_ADAPTIVE_DECODER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Default nominal cell time (MFM DD) */
#define UFT_ADEC_NOMINAL_CELL_NS    4000

/** Default tolerance (±10%) */
#define UFT_ADEC_DEFAULT_TOLERANCE  0.10

/** Maximum clusters for K-means */
#define UFT_ADEC_MAX_CLUSTERS       4

/** Minimum samples for clustering */
#define UFT_ADEC_MIN_CLUSTER_SAMPLES 100

/** Standard RPM values */
#define UFT_ADEC_RPM_300            300.0
#define UFT_ADEC_RPM_360            360.0

/** Revolution time in ns */
#define UFT_ADEC_REV_NS_300         200000000
#define UFT_ADEC_REV_NS_360         166666667

/*===========================================================================
 * Encoding Types
 *===========================================================================*/

/**
 * @brief Encoding mode
 */
typedef enum {
    UFT_ADEC_MANCHESTER,        /**< Standard Manchester encoding */
    UFT_ADEC_VARIABLE_RLL,      /**< Variable-length RLL-like */
    UFT_ADEC_MFM,               /**< Modified FM */
    UFT_ADEC_FM,                /**< Frequency Modulation */
    UFT_ADEC_GCR,               /**< Group Coded Recording */
    UFT_ADEC_AUTO               /**< Auto-detect from clustering */
} uft_adec_mode_t;

/**
 * @brief Interval classification
 */
typedef enum {
    UFT_ADEC_INT_HALF,          /**< Half cell (Manchester 0-bit part) */
    UFT_ADEC_INT_FULL,          /**< Full cell (Manchester 1-bit) */
    UFT_ADEC_INT_SHORT,         /**< Short (RLL 0-bit) */
    UFT_ADEC_INT_LONG,          /**< Long (RLL 1-bit) */
    UFT_ADEC_INT_HALF_SHORT,    /**< Half-short (variable mode) */
    UFT_ADEC_INT_UNKNOWN        /**< Unknown/noise */
} uft_adec_interval_t;

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/**
 * @brief Cluster center (from K-means)
 */
typedef struct {
    double center_ns;           /**< Cluster center in ns */
    uint32_t count;             /**< Number of samples in cluster */
    double variance;            /**< Within-cluster variance */
    uft_adec_interval_t type;   /**< Assigned interval type */
} uft_adec_cluster_t;

/**
 * @brief Adaptive thresholds
 */
typedef struct {
    double half_cell_ns;        /**< Half-cell threshold (Manchester) */
    double full_cell_ns;        /**< Full-cell threshold (Manchester) */
    double short_cell_ns;       /**< Short threshold (variable) */
    double long_cell_ns;        /**< Long threshold (variable) */
    double half_short_ns;       /**< Half-short threshold (variable) */
    double tolerance;           /**< Relative tolerance (0-1) */
    bool from_clustering;       /**< Thresholds from clustering */
} uft_adec_thresholds_t;

/**
 * @brief Decoder configuration
 */
typedef struct {
    uft_adec_mode_t mode;       /**< Encoding mode */
    double nominal_cell_ns;     /**< Nominal cell time */
    double density;             /**< Density factor (1.0 = normal) */
    double rpm;                 /**< Drive RPM for normalization */
    double tolerance;           /**< Interval tolerance (0-1) */
    bool use_clustering;        /**< Use K-means for thresholds */
    uint8_t num_clusters;       /**< Number of K-means clusters */
    bool normalize_rpm;         /**< Normalize to expected RPM */
} uft_adec_config_t;

/**
 * @brief Decoder state
 */
typedef struct {
    uft_adec_config_t config;
    uft_adec_thresholds_t thresholds;
    uft_adec_cluster_t clusters[UFT_ADEC_MAX_CLUSTERS];
    uint8_t cluster_count;
    
    /* Statistics */
    double mean_interval_ns;
    double std_interval_ns;
    uint32_t total_intervals;
    uint32_t decoded_bits;
    uint32_t error_count;
} uft_adec_state_t;

/**
 * @brief Decode result
 */
typedef struct {
    uint8_t *data;              /**< Decoded bytes (caller allocates) */
    size_t byte_count;          /**< Number of bytes decoded */
    size_t bit_count;           /**< Number of bits decoded */
    uint32_t error_count;       /**< Classification errors */
    double confidence;          /**< Overall decode confidence */
} uft_adec_result_t;

/*===========================================================================
 * Initialization
 *===========================================================================*/

/**
 * @brief Initialize decoder configuration with defaults
 * @param config Configuration to initialize
 * @param mode Encoding mode
 */
void uft_adec_config_init(uft_adec_config_t *config, uft_adec_mode_t mode);

/**
 * @brief Initialize decoder state
 * @param state Decoder state
 * @param config Configuration
 * @return 0 on success
 */
int uft_adec_init(uft_adec_state_t *state, const uft_adec_config_t *config);

/**
 * @brief Reset decoder state
 * @param state Decoder state
 */
void uft_adec_reset(uft_adec_state_t *state);

/*===========================================================================
 * K-Means Clustering
 *===========================================================================*/

/**
 * @brief Perform K-means clustering on flux intervals
 * @param intervals Flux intervals in ns
 * @param count Number of intervals
 * @param k Number of clusters (2-4)
 * @param clusters Output cluster centers (sorted ascending)
 * @param max_iterations Maximum K-means iterations
 * @return Number of clusters found
 * 
 * Clusters are used to determine adaptive thresholds:
 * - 2 clusters: half/full (Manchester)
 * - 3 clusters: half_short/short/long (variable RLL)
 */
uint8_t uft_adec_kmeans(const uint32_t *intervals, size_t count, uint8_t k,
                        uft_adec_cluster_t *clusters, uint16_t max_iterations);

/**
 * @brief Update thresholds from cluster centers
 * @param state Decoder state
 * @param clusters Cluster centers
 * @param cluster_count Number of clusters
 */
void uft_adec_update_thresholds(uft_adec_state_t *state,
                                const uft_adec_cluster_t *clusters,
                                uint8_t cluster_count);

/**
 * @brief Auto-detect encoding mode from clusters
 * @param clusters Cluster centers
 * @param cluster_count Number of clusters
 * @return Detected encoding mode
 */
uft_adec_mode_t uft_adec_detect_mode(const uft_adec_cluster_t *clusters,
                                      uint8_t cluster_count);

/*===========================================================================
 * Interval Classification
 *===========================================================================*/

/**
 * @brief Classify a single interval
 * @param state Decoder state
 * @param interval_ns Interval in nanoseconds
 * @return Interval type
 */
uft_adec_interval_t uft_adec_classify(const uft_adec_state_t *state,
                                       uint32_t interval_ns);

/**
 * @brief Classify with confidence
 * @param state Decoder state
 * @param interval_ns Interval in nanoseconds
 * @param confidence Output: classification confidence (0-1)
 * @return Interval type
 */
uft_adec_interval_t uft_adec_classify_conf(const uft_adec_state_t *state,
                                            uint32_t interval_ns,
                                            double *confidence);

/*===========================================================================
 * Decoding Functions
 *===========================================================================*/

/**
 * @brief Learn thresholds from flux data
 * @param state Decoder state
 * @param intervals Flux intervals
 * @param count Number of intervals
 * @return 0 on success
 * 
 * Should be called before decode_flux() to learn adaptive thresholds.
 */
int uft_adec_learn(uft_adec_state_t *state, const uint32_t *intervals,
                   size_t count);

/**
 * @brief Decode flux intervals to bits/bytes
 * @param state Decoder state
 * @param intervals Flux intervals
 * @param count Number of intervals
 * @param result Output decode result
 * @return 0 on success
 */
int uft_adec_decode(uft_adec_state_t *state, const uint32_t *intervals,
                    size_t count, uft_adec_result_t *result);

/**
 * @brief Decode Manchester encoded flux
 * @param state Decoder state
 * @param intervals Flux intervals
 * @param count Number of intervals
 * @param bits Output bit array (caller allocates)
 * @param max_bits Maximum bits to decode
 * @return Number of bits decoded
 */
size_t uft_adec_decode_manchester(const uft_adec_state_t *state,
                                  const uint32_t *intervals, size_t count,
                                  uint8_t *bits, size_t max_bits);

/**
 * @brief Decode variable-length RLL flux
 * @param state Decoder state
 * @param intervals Flux intervals
 * @param count Number of intervals
 * @param bits Output bit array
 * @param max_bits Maximum bits to decode
 * @return Number of bits decoded
 */
size_t uft_adec_decode_variable(const uft_adec_state_t *state,
                                const uint32_t *intervals, size_t count,
                                uint8_t *bits, size_t max_bits);

/*===========================================================================
 * RPM Normalization
 *===========================================================================*/

/**
 * @brief Normalize intervals to target RPM
 * @param intervals Flux intervals (modified in place)
 * @param count Number of intervals
 * @param measured_rpm Measured/estimated RPM
 * @param target_rpm Target RPM for normalization
 * @return Scale factor applied
 */
double uft_adec_normalize_rpm(uint32_t *intervals, size_t count,
                              double measured_rpm, double target_rpm);

/**
 * @brief Estimate RPM from revolution data
 * @param rev_intervals Revolution intervals
 * @param rev_count Number of revolutions
 * @return Estimated RPM
 */
double uft_adec_estimate_rpm(const uint64_t *rev_intervals, size_t rev_count);

/*===========================================================================
 * Statistics
 *===========================================================================*/

/**
 * @brief Compute interval statistics
 * @param intervals Flux intervals
 * @param count Number of intervals
 * @param mean Output: mean interval
 * @param std Output: standard deviation
 * @param min_val Output: minimum interval
 * @param max_val Output: maximum interval
 */
void uft_adec_statistics(const uint32_t *intervals, size_t count,
                         double *mean, double *std,
                         uint32_t *min_val, uint32_t *max_val);

/**
 * @brief Get decode statistics
 * @param state Decoder state
 * @param total_bits Output: total bits decoded
 * @param error_rate Output: error rate (0-1)
 * @param mean_confidence Output: mean classification confidence
 */
void uft_adec_get_stats(const uft_adec_state_t *state,
                        uint32_t *total_bits, double *error_rate,
                        double *mean_confidence);

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Pack bits to bytes
 * @param bits Bit array
 * @param bit_count Number of bits
 * @param bytes Output byte array
 * @return Number of bytes written
 */
static inline size_t uft_adec_pack_bits(const uint8_t *bits, size_t bit_count,
                                         uint8_t *bytes)
{
    size_t byte_count = bit_count / 8;
    for (size_t i = 0; i < byte_count; i++) {
        bytes[i] = 0;
        for (int b = 0; b < 8; b++) {
            bytes[i] = (bytes[i] << 1) | (bits[i * 8 + b] & 1);
        }
    }
    return byte_count;
}

/**
 * @brief Get encoding mode name
 * @param mode Encoding mode
 * @return Name string
 */
const char *uft_adec_mode_name(uft_adec_mode_t mode);

/**
 * @brief Get interval type name
 * @param type Interval type
 * @return Name string
 */
const char *uft_adec_interval_name(uft_adec_interval_t type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ADAPTIVE_DECODER_H */
