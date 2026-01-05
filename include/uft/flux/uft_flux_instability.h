/**
 * @file uft_flux_instability.h
 * @brief Flux Instability Analysis and Scoring
 * 
 * Based on FloppyAI flux_analyzer.py instability algorithms
 * Ported from Python to C for UFT
 * 
 * Provides metrics for analyzing flux stability across revolutions:
 * - Phase variance (angular jitter across revolutions)
 * - Cross-revolution coherence (correlation to mean profile)
 * - Outlier/gap rate detection
 * - Combined instability score
 * 
 * These metrics are essential for:
 * - Detecting weak/fuzzy bits
 * - Identifying media degradation
 * - Copy protection analysis
 * - Quality assessment of flux captures
 */

#ifndef UFT_FLUX_INSTABILITY_H
#define UFT_FLUX_INSTABILITY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Revolution time constants (nanoseconds) */
#define UFT_REV_TIME_NS_300     200000000   /**< ~200ms at 300 RPM */
#define UFT_REV_TIME_NS_360     166666667   /**< ~166.67ms at 360 RPM */

/** Default angular bins for analysis */
#define UFT_INSTAB_DEFAULT_BINS     360
#define UFT_INSTAB_HIGH_RES_BINS    1440

/** Instability weight factors (from FloppyAI) */
#define UFT_INSTAB_W_VARIANCE       0.4     /**< Phase variance weight */
#define UFT_INSTAB_W_INCOHERENCE    0.3     /**< Cross-rev incoherence weight */
#define UFT_INSTAB_W_GAP_RATE       0.2     /**< Gap rate weight */
#define UFT_INSTAB_W_OUTLIER        0.1     /**< Outlier rate weight */

/** Threshold multipliers */
#define UFT_INSTAB_SHORT_THRESHOLD  0.5     /**< Short interval = 0.5 * mean */
#define UFT_INSTAB_LONG_THRESHOLD   3.0     /**< Long interval = mean + 3*std */
#define UFT_INSTAB_GAP_THRESHOLD    4.0     /**< Gap = mean + 4*std or 2.5*mean */

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/**
 * @brief Angular histogram bin
 */
typedef struct {
    double density;             /**< Normalized flux density in this bin */
    double variance;            /**< Variance across revolutions */
} uft_angular_bin_t;

/**
 * @brief Per-revolution statistics
 */
typedef struct {
    uint32_t flux_count;        /**< Number of flux transitions */
    double total_time_ns;       /**< Total revolution time */
    double mean_interval;       /**< Mean flux interval */
    double std_interval;        /**< Std dev of intervals */
    int phase_shift_bins;       /**< Phase shift for alignment */
} uft_rev_stats_t;

/**
 * @brief Instability feature set
 */
typedef struct {
    double phase_var_p95;       /**< 95th percentile of angular phase variance */
    double phase_incoherence;   /**< 1 - mean(correlation to mean profile) */
    double outlier_rate;        /**< Rate of short/long interval outliers */
    double gap_rate;            /**< Rate of very long (gap) intervals */
} uft_instab_features_t;

/**
 * @brief Complete instability analysis result
 */
typedef struct {
    /* Angular analysis */
    uint16_t angular_bins;      /**< Number of bins used */
    double *angular_hist;       /**< Normalized histogram [angular_bins] */
    double *per_angle_variance; /**< Variance per bin [angular_bins] */
    
    /* Revolution analysis */
    uint16_t rev_count;         /**< Number of revolutions analyzed */
    uft_rev_stats_t *rev_stats; /**< Per-revolution statistics [rev_count] */
    int *rev_phase_shifts;      /**< Phase alignment shifts [rev_count] */
    
    /* Instability metrics */
    uft_instab_features_t features;
    double instability_score;   /**< Combined 0.0-1.0 score */
    
    /* Interval statistics */
    double mean_interval_ns;
    double std_interval_ns;
    uint32_t total_fluxes;
    double density_estimate;    /**< Estimated bits per revolution */
    
    /* Anomaly detection */
    uint32_t anomaly_count;     /**< Number of anomalies detected */
    uint32_t *anomaly_positions;/**< Positions of anomalies */
} uft_instab_result_t;

/**
 * @brief Instability analysis configuration
 */
typedef struct {
    uint16_t angular_bins;      /**< Angular resolution (default: 360) */
    double rpm;                 /**< Drive RPM (300 or 360) */
    bool align_revolutions;     /**< Perform phase alignment */
    bool compute_histogram;     /**< Compute interval histogram */
    uint16_t hist_bins;         /**< Histogram bins (log-spaced) */
    double hist_min_ns;         /**< Histogram min (ns) */
    double hist_max_ns;         /**< Histogram max (ns) */
} uft_instab_config_t;

/*===========================================================================
 * Initialization
 *===========================================================================*/

/**
 * @brief Initialize config with defaults
 * @param config Configuration to initialize
 */
static inline void uft_instab_config_default(uft_instab_config_t *config)
{
    config->angular_bins = UFT_INSTAB_DEFAULT_BINS;
    config->rpm = 300.0;
    config->align_revolutions = true;
    config->compute_histogram = true;
    config->hist_bins = 100;
    config->hist_min_ns = 1000;
    config->hist_max_ns = 100000;
}

/**
 * @brief Allocate result structure
 * @param angular_bins Number of angular bins
 * @param max_revs Maximum revolutions
 * @return Allocated result or NULL
 */
uft_instab_result_t *uft_instab_alloc(uint16_t angular_bins, uint16_t max_revs);

/**
 * @brief Free result structure
 * @param result Result to free
 */
void uft_instab_free(uft_instab_result_t *result);

/*===========================================================================
 * Core Analysis Functions
 *===========================================================================*/

/**
 * @brief Analyze flux instability across revolutions
 * @param revolutions Array of revolution data (array of arrays)
 * @param rev_counts Size of each revolution array
 * @param rev_count Number of revolutions
 * @param config Analysis configuration
 * @param result Output result structure
 * @return 0 on success
 * 
 * This is the main analysis function that computes:
 * 1. Per-revolution statistics
 * 2. Angular histograms with phase alignment
 * 3. Cross-revolution coherence
 * 4. Outlier/gap detection
 * 5. Combined instability score
 */
int uft_instab_analyze(const uint32_t **revolutions, const size_t *rev_counts,
                       size_t rev_count, const uft_instab_config_t *config,
                       uft_instab_result_t *result);

/**
 * @brief Compute instability score from features
 * @param features Instability features
 * @return Score 0.0-1.0 (0=stable, 1=unstable)
 */
static inline double uft_instab_compute_score(const uft_instab_features_t *f)
{
    double score = UFT_INSTAB_W_VARIANCE * fmin(1.0, f->phase_var_p95)
                 + UFT_INSTAB_W_INCOHERENCE * f->phase_incoherence
                 + UFT_INSTAB_W_GAP_RATE * f->gap_rate
                 + UFT_INSTAB_W_OUTLIER * f->outlier_rate;
    return fmin(1.0, fmax(0.0, score));
}

/*===========================================================================
 * Angular Analysis
 *===========================================================================*/

/**
 * @brief Build angular histogram from flux intervals
 * @param intervals Flux intervals in nanoseconds
 * @param count Number of intervals
 * @param bins Number of angular bins
 * @param histogram Output histogram [bins] (caller allocates)
 * @return Total revolution time in ns
 */
double uft_instab_angular_histogram(const uint32_t *intervals, size_t count,
                                    uint16_t bins, double *histogram);

/**
 * @brief Find optimal phase shift for alignment
 * @param hist Histogram to align
 * @param ref_hist Reference histogram
 * @param bins Number of bins
 * @return Optimal shift in bins
 * 
 * Uses cross-correlation to find optimal alignment.
 */
int uft_instab_find_phase_shift(const double *hist, const double *ref_hist,
                                uint16_t bins);

/**
 * @brief Compute per-angle variance across revolutions
 * @param histograms Array of aligned histograms [rev_count][bins]
 * @param rev_count Number of revolutions
 * @param bins Number of bins
 * @param variance Output variance array [bins]
 */
void uft_instab_angular_variance(const double **histograms, size_t rev_count,
                                 uint16_t bins, double *variance);

/*===========================================================================
 * Cross-Revolution Analysis
 *===========================================================================*/

/**
 * @brief Compute mean histogram profile
 * @param histograms Array of histograms [rev_count][bins]
 * @param rev_count Number of revolutions
 * @param bins Number of bins
 * @param mean_hist Output mean histogram [bins]
 */
void uft_instab_mean_profile(const double **histograms, size_t rev_count,
                             uint16_t bins, double *mean_hist);

/**
 * @brief Compute correlation between two histograms
 * @param a First histogram
 * @param b Second histogram
 * @param bins Number of bins
 * @return Pearson correlation coefficient (-1 to 1)
 */
double uft_instab_correlation(const double *a, const double *b, uint16_t bins);

/**
 * @brief Compute phase incoherence
 * @param histograms Aligned histograms [rev_count][bins]
 * @param rev_count Number of revolutions
 * @param bins Number of bins
 * @return Incoherence (1 - mean_correlation), range 0-1
 */
double uft_instab_phase_incoherence(const double **histograms, size_t rev_count,
                                    uint16_t bins);

/*===========================================================================
 * Outlier Detection
 *===========================================================================*/

/**
 * @brief Compute outlier and gap rates
 * @param intervals All flux intervals
 * @param count Number of intervals
 * @param mean Mean interval
 * @param std Standard deviation
 * @param outlier_rate Output: rate of outliers
 * @param gap_rate Output: rate of gaps
 */
void uft_instab_outlier_rates(const uint32_t *intervals, size_t count,
                              double mean, double std,
                              double *outlier_rate, double *gap_rate);

/**
 * @brief Detect anomalous intervals
 * @param intervals Flux intervals
 * @param count Number of intervals
 * @param mean Mean interval
 * @param std Standard deviation
 * @param positions Output: anomaly positions (caller allocates)
 * @param max_anomalies Maximum anomalies to detect
 * @return Number of anomalies found
 */
size_t uft_instab_detect_anomalies(const uint32_t *intervals, size_t count,
                                   double mean, double std,
                                   uint32_t *positions, size_t max_anomalies);

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Get revolution time for RPM
 * @param rpm Drive RPM (300 or 360)
 * @return Revolution time in nanoseconds
 */
static inline uint64_t uft_instab_rev_time_ns(double rpm)
{
    return (uint64_t)(60000000000.0 / rpm);
}

/**
 * @brief Estimate density (bits per revolution)
 * @param total_intervals Total flux transitions
 * @param rev_count Number of revolutions
 * @return Estimated bits per revolution
 */
static inline double uft_instab_density_estimate(size_t total_intervals,
                                                  size_t rev_count)
{
    if (rev_count == 0) return 0.0;
    return (double)total_intervals / (double)rev_count;
}

/**
 * @brief Classify instability level
 * @param score Instability score (0-1)
 * @return Human-readable classification
 */
static inline const char *uft_instab_classify(double score)
{
    if (score < 0.1) return "Excellent";
    if (score < 0.2) return "Good";
    if (score < 0.4) return "Fair";
    if (score < 0.6) return "Poor";
    if (score < 0.8) return "Critical";
    return "Unreadable";
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLUX_INSTABILITY_H */
