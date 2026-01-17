/**
 * @file uft_flux_analysis.h
 * @brief Flux Timing Analysis Module
 * 
 * Advanced flux transition analysis for disk preservation:
 * - Bit cell timing statistics
 * - Jitter analysis
 * - Speed variation detection
 * - Flux histogram generation
 * - Protection detection via timing anomalies
 * 
 * Supports: Kryoflux, SuperCard Pro, Greaseweazle flux data
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#ifndef UFT_FLUX_ANALYSIS_H
#define UFT_FLUX_ANALYSIS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Sample rates */
#define FLUX_SAMPLE_RATE_KRYOFLUX   (24027428)  /**< Kryoflux: ~24 MHz */
#define FLUX_SAMPLE_RATE_SCP        (40000000)  /**< SCP: 40 MHz */
#define FLUX_SAMPLE_RATE_GW         (80000000)  /**< Greaseweazle: 80 MHz */

/** Bit cell times (in nanoseconds) */
#define FLUX_MFM_CELL_NS            2000        /**< MFM: 2µs */
#define FLUX_FM_CELL_NS             4000        /**< FM: 4µs */
#define FLUX_GCR_C64_CELL_NS        3250        /**< C64 GCR: ~3.25µs */
#define FLUX_GCR_APPLE_CELL_NS      4000        /**< Apple GCR: 4µs */

/** Histogram bins */
#define FLUX_HISTOGRAM_BINS         256
#define FLUX_HISTOGRAM_MAX_NS       16000       /**< Max timing in histogram */

/** Analysis thresholds */
#define FLUX_JITTER_LOW             5           /**< Low jitter threshold (%) */
#define FLUX_JITTER_HIGH            15          /**< High jitter threshold (%) */
#define FLUX_SPEED_VARIATION_MAX    3.0f        /**< Max speed variation (%) */

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Flux encoding type
 */
typedef enum {
    FLUX_ENCODING_UNKNOWN   = 0,
    FLUX_ENCODING_FM        = 1,    /**< Frequency Modulation */
    FLUX_ENCODING_MFM       = 2,    /**< Modified FM */
    FLUX_ENCODING_GCR_C64   = 3,    /**< C64/1541 GCR */
    FLUX_ENCODING_GCR_APPLE = 4,    /**< Apple II GCR */
    FLUX_ENCODING_AMIGA     = 5,    /**< Amiga MFM */
    FLUX_ENCODING_RAW       = 6,    /**< Raw (unencoded) */
} flux_encoding_t;

/**
 * @brief Flux source type
 */
typedef enum {
    FLUX_SOURCE_UNKNOWN     = 0,
    FLUX_SOURCE_KRYOFLUX    = 1,
    FLUX_SOURCE_SCP         = 2,
    FLUX_SOURCE_GREASEWEAZLE = 3,
    FLUX_SOURCE_HXC         = 4,
    FLUX_SOURCE_APPLESAUCE  = 5,
} flux_source_t;

/**
 * @brief Flux timing histogram
 */
typedef struct {
    uint32_t    bins[FLUX_HISTOGRAM_BINS];  /**< Bin counts */
    uint32_t    total_samples;              /**< Total samples */
    uint32_t    min_time_ns;                /**< Minimum timing */
    uint32_t    max_time_ns;                /**< Maximum timing */
    uint32_t    peak_bins[4];               /**< Peak bin indices */
    int         num_peaks;                  /**< Number of peaks found */
} flux_histogram_t;

/**
 * @brief Bit cell statistics
 */
typedef struct {
    float       mean_ns;            /**< Mean cell time (ns) */
    float       stddev_ns;          /**< Standard deviation (ns) */
    float       jitter_percent;     /**< Jitter as percentage */
    uint32_t    sample_count;       /**< Number of samples */
    uint32_t    min_ns;             /**< Minimum time */
    uint32_t    max_ns;             /**< Maximum time */
    uint32_t    outliers;           /**< Outlier count */
} flux_cell_stats_t;

/**
 * @brief Flux transition data
 */
typedef struct {
    uint32_t    *times;             /**< Transition times (in sample units) */
    size_t      num_transitions;    /**< Number of transitions */
    size_t      capacity;           /**< Allocated capacity */
    uint32_t    sample_rate;        /**< Sample rate (Hz) */
    flux_source_t source;           /**< Data source */
} flux_transitions_t;

/**
 * @brief Revolution data
 */
typedef struct {
    size_t      start_index;        /**< Start index in transitions */
    size_t      num_transitions;    /**< Transitions in this revolution */
    uint32_t    duration_ns;        /**< Revolution duration (ns) */
    float       rpm;                /**< Calculated RPM */
} flux_revolution_t;

/**
 * @brief Track analysis result
 */
typedef struct {
    /* Basic info */
    int                 track;              /**< Track number */
    int                 side;               /**< Side (0 or 1) */
    flux_encoding_t     encoding;           /**< Detected encoding */
    
    /* Revolution data */
    flux_revolution_t   revolutions[16];    /**< Per-revolution data */
    int                 num_revolutions;    /**< Number of revolutions */
    
    /* Timing statistics */
    flux_cell_stats_t   cell_stats;         /**< Bit cell statistics */
    flux_histogram_t    histogram;          /**< Timing histogram */
    
    /* Speed analysis */
    float               rpm_mean;           /**< Mean RPM */
    float               rpm_stddev;         /**< RPM standard deviation */
    float               speed_variation;    /**< Speed variation (%) */
    
    /* Quality metrics */
    float               signal_quality;     /**< Signal quality (0-100) */
    int                 weak_bits;          /**< Weak bit regions */
    int                 missing_clocks;     /**< Missing clock bits */
    int                 extra_clocks;       /**< Extra clock bits */
    
    /* Protection indicators */
    bool                has_long_track;     /**< Longer than normal */
    bool                has_short_track;    /**< Shorter than normal */
    bool                has_density_change; /**< Density changes mid-track */
    bool                has_weak_region;    /**< Has weak/fuzzy bits */
    bool                has_no_flux;        /**< Has no-flux region */
    bool                has_timing_anomaly; /**< Unusual timing pattern */
    
    /* Description */
    char                description[256];   /**< Analysis description */
} flux_track_analysis_t;

/**
 * @brief Disk analysis result
 */
typedef struct {
    /* Basic info */
    int                     num_tracks;     /**< Number of tracks */
    int                     num_sides;      /**< Number of sides */
    flux_encoding_t         encoding;       /**< Primary encoding */
    flux_source_t           source;         /**< Data source */
    
    /* Track analyses */
    flux_track_analysis_t   *tracks;        /**< Track analyses */
    
    /* Disk-wide statistics */
    float                   avg_rpm;        /**< Average RPM */
    float                   avg_jitter;     /**< Average jitter */
    float                   signal_quality; /**< Overall quality */
    
    /* Protection detection */
    int                     protection_tracks;  /**< Tracks with protection */
    bool                    has_protections;    /**< Protections detected */
    
    /* Summary */
    char                    summary[512];   /**< Analysis summary */
} flux_disk_analysis_t;

/* ============================================================================
 * API Functions - Transition Management
 * ============================================================================ */

/**
 * @brief Create flux transitions structure
 * @param sample_rate Sample rate in Hz
 * @param source Data source type
 * @return New structure, or NULL on error
 */
flux_transitions_t *flux_create_transitions(uint32_t sample_rate, flux_source_t source);

/**
 * @brief Free flux transitions
 * @param trans Transitions to free
 */
void flux_free_transitions(flux_transitions_t *trans);

/**
 * @brief Add transition to list
 * @param trans Transitions structure
 * @param time Transition time (sample units)
 * @return 0 on success
 */
int flux_add_transition(flux_transitions_t *trans, uint32_t time);

/**
 * @brief Load transitions from raw flux data
 * @param data Raw flux data
 * @param size Data size
 * @param source Data source type
 * @param trans Output transitions
 * @return 0 on success
 */
int flux_load_raw(const uint8_t *data, size_t size, flux_source_t source,
                  flux_transitions_t **trans);

/* ============================================================================
 * API Functions - Basic Analysis
 * ============================================================================ */

/**
 * @brief Calculate bit cell statistics
 * @param trans Flux transitions
 * @param encoding Expected encoding
 * @param stats Output statistics
 * @return 0 on success
 */
int flux_calc_cell_stats(const flux_transitions_t *trans, flux_encoding_t encoding,
                         flux_cell_stats_t *stats);

/**
 * @brief Generate timing histogram
 * @param trans Flux transitions
 * @param histogram Output histogram
 * @return 0 on success
 */
int flux_generate_histogram(const flux_transitions_t *trans, flux_histogram_t *histogram);

/**
 * @brief Find histogram peaks
 * @param histogram Histogram to analyze
 * @param max_peaks Maximum peaks to find
 * @return Number of peaks found
 */
int flux_find_histogram_peaks(flux_histogram_t *histogram, int max_peaks);

/**
 * @brief Detect encoding type from flux data
 * @param trans Flux transitions
 * @return Detected encoding type
 */
flux_encoding_t flux_detect_encoding(const flux_transitions_t *trans);

/* ============================================================================
 * API Functions - Revolution Analysis
 * ============================================================================ */

/**
 * @brief Find index marks (revolution boundaries)
 * @param trans Flux transitions
 * @param revolutions Output revolution data
 * @param max_revolutions Maximum revolutions to find
 * @return Number of revolutions found
 */
int flux_find_revolutions(const flux_transitions_t *trans,
                          flux_revolution_t *revolutions, int max_revolutions);

/**
 * @brief Calculate RPM from revolution
 * @param rev Revolution data
 * @param sample_rate Sample rate
 * @return RPM value
 */
float flux_calc_rpm(const flux_revolution_t *rev, uint32_t sample_rate);

/**
 * @brief Analyze speed variation across revolutions
 * @param revolutions Revolution data
 * @param num_revolutions Number of revolutions
 * @param mean_rpm Output: mean RPM
 * @param variation Output: variation percentage
 * @return 0 on success
 */
int flux_analyze_speed(const flux_revolution_t *revolutions, int num_revolutions,
                       float *mean_rpm, float *variation);

/* ============================================================================
 * API Functions - Track Analysis
 * ============================================================================ */

/**
 * @brief Analyze single track
 * @param trans Flux transitions
 * @param track Track number
 * @param side Side (0 or 1)
 * @param analysis Output analysis
 * @return 0 on success
 */
int flux_analyze_track(const flux_transitions_t *trans, int track, int side,
                       flux_track_analysis_t *analysis);

/**
 * @brief Check for weak bit regions
 * @param trans Flux transitions
 * @param threshold Weakness threshold
 * @param regions Output: number of weak regions
 * @return Total weak bits detected
 */
int flux_find_weak_bits(const flux_transitions_t *trans, int threshold, int *regions);

/**
 * @brief Check for no-flux regions
 * @param trans Flux transitions
 * @param min_gap_ns Minimum gap to detect (ns)
 * @param positions Output positions (can be NULL)
 * @param max_positions Maximum positions to return
 * @return Number of no-flux regions
 */
int flux_find_no_flux(const flux_transitions_t *trans, uint32_t min_gap_ns,
                      size_t *positions, int max_positions);

/**
 * @brief Detect timing anomalies
 * @param trans Flux transitions
 * @param encoding Expected encoding
 * @param anomalies Output: anomaly count
 * @return true if significant anomalies found
 */
bool flux_detect_anomalies(const flux_transitions_t *trans, flux_encoding_t encoding,
                           int *anomalies);

/* ============================================================================
 * API Functions - Disk Analysis
 * ============================================================================ */

/**
 * @brief Create disk analysis structure
 * @param num_tracks Number of tracks
 * @param num_sides Number of sides
 * @return New structure, or NULL on error
 */
flux_disk_analysis_t *flux_create_disk_analysis(int num_tracks, int num_sides);

/**
 * @brief Free disk analysis
 * @param analysis Analysis to free
 */
void flux_free_disk_analysis(flux_disk_analysis_t *analysis);

/**
 * @brief Analyze complete disk
 * @param track_trans Array of track transitions
 * @param num_tracks Number of tracks
 * @param num_sides Number of sides
 * @param analysis Output analysis
 * @return 0 on success
 */
int flux_analyze_disk(flux_transitions_t **track_trans, int num_tracks, int num_sides,
                      flux_disk_analysis_t *analysis);

/**
 * @brief Generate disk analysis report
 * @param analysis Disk analysis
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Characters written
 */
int flux_generate_report(const flux_disk_analysis_t *analysis,
                         char *buffer, size_t buffer_size);

/* ============================================================================
 * API Functions - Protection Detection via Flux
 * ============================================================================ */

/**
 * @brief Check for copy protection via flux analysis
 * @param analysis Track analysis
 * @param description Output description
 * @param desc_size Description buffer size
 * @return true if protection detected
 */
bool flux_detect_protection(const flux_track_analysis_t *analysis,
                            char *description, size_t desc_size);

/**
 * @brief Check for long track protection
 * @param trans Flux transitions
 * @param expected_length_ns Expected track length
 * @param tolerance_percent Tolerance percentage
 * @return Difference from expected (negative = short)
 */
int flux_check_track_length(const flux_transitions_t *trans,
                            uint32_t expected_length_ns, float tolerance_percent);

/**
 * @brief Check for density variation protection
 * @param trans Flux transitions
 * @param encoding Expected encoding
 * @param num_changes Output: number of density changes
 * @return true if protection detected
 */
bool flux_check_density_protection(const flux_transitions_t *trans,
                                   flux_encoding_t encoding, int *num_changes);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Convert sample units to nanoseconds
 * @param samples Sample count
 * @param sample_rate Sample rate (Hz)
 * @return Time in nanoseconds
 */
uint32_t flux_samples_to_ns(uint32_t samples, uint32_t sample_rate);

/**
 * @brief Convert nanoseconds to sample units
 * @param ns Time in nanoseconds
 * @param sample_rate Sample rate (Hz)
 * @return Sample count
 */
uint32_t flux_ns_to_samples(uint32_t ns, uint32_t sample_rate);

/**
 * @brief Get encoding name
 * @param encoding Encoding type
 * @return Encoding name string
 */
const char *flux_encoding_name(flux_encoding_t encoding);

/**
 * @brief Get source name
 * @param source Source type
 * @return Source name string
 */
const char *flux_source_name(flux_source_t source);

/**
 * @brief Get expected bit cell time
 * @param encoding Encoding type
 * @return Expected cell time (ns)
 */
uint32_t flux_expected_cell_time(flux_encoding_t encoding);

/**
 * @brief Print histogram to file
 * @param histogram Histogram to print
 * @param fp Output file
 */
void flux_print_histogram(const flux_histogram_t *histogram, FILE *fp);

/**
 * @brief Print track analysis to file
 * @param analysis Track analysis
 * @param fp Output file
 */
void flux_print_track_analysis(const flux_track_analysis_t *analysis, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLUX_ANALYSIS_H */
