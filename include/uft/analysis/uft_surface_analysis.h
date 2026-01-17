/**
 * @file uft_surface_analysis.h
 * @brief Disk Surface Analysis Types and Functions
 * 
 * EXT4-007: Comprehensive disk surface analysis
 */

#ifndef UFT_SURFACE_ANALYSIS_H
#define UFT_SURFACE_ANALYSIS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

#define UFT_MAX_TIMING_PEAKS    8
#define UFT_MAX_ANOMALIES       64

/*===========================================================================
 * Anomaly Type Enumeration
 *===========================================================================*/

typedef enum {
    UFT_ANOMALY_NONE = 0,
    UFT_ANOMALY_DROPOUT,        /**< Missing flux transitions */
    UFT_ANOMALY_SPIKE,          /**< Spurious transitions */
    UFT_ANOMALY_WEAK,           /**< Weak signal area */
    UFT_ANOMALY_NOISE           /**< High noise area */
} uft_anomaly_type_t;

/*===========================================================================
 * Defect Type Enumeration
 *===========================================================================*/

typedef enum {
    UFT_DEFECT_NONE = 0,
    UFT_DEFECT_QUALITY,         /**< Low quality track */
    UFT_DEFECT_DROPOUT,         /**< Signal dropout */
    UFT_DEFECT_SCRATCH,         /**< Physical scratch */
    UFT_DEFECT_WEAK,            /**< Weak magnetic area */
    UFT_DEFECT_ALIGNMENT        /**< Alignment issue */
} uft_defect_type_t;

/*===========================================================================
 * Track Surface Analysis Result
 *===========================================================================*/

typedef struct {
    bool     valid;                             /**< Analysis valid */
    size_t   flux_count;                        /**< Number of flux transitions */
    double   track_time_us;                     /**< Total track time in microseconds */
    double   min_pulse_us;                      /**< Minimum pulse width */
    double   max_pulse_us;                      /**< Maximum pulse width */
    double   mean_pulse_us;                     /**< Mean pulse width */
    size_t   flux_density;                      /**< Flux transitions per rotation */
    double   estimated_data_rate;               /**< Estimated bits per second */
    int      timing_peak_count;                 /**< Number of timing peaks found */
    double   timing_peaks[UFT_MAX_TIMING_PEAKS];/**< Timing peak positions */
    size_t   anomaly_count;                     /**< Number of anomalies detected */
    size_t   anomaly_positions[UFT_MAX_ANOMALIES]; /**< Anomaly positions */
    uft_anomaly_type_t anomaly_types[UFT_MAX_ANOMALIES]; /**< Anomaly types */
    double   quality_score;                     /**< Track quality 0-100 */
} uft_track_surface_t;

/*===========================================================================
 * Surface Map (Multi-Track)
 *===========================================================================*/

typedef struct {
    int      tracks;                            /**< Number of tracks */
    int      sides;                             /**< Number of sides */
    uft_track_surface_t *track_data;            /**< Track data array */
} uft_surface_map_t;

/*===========================================================================
 * Surface Statistics
 *===========================================================================*/

typedef struct {
    int      total_tracks;                      /**< Total track count */
    int      analyzed_tracks;                   /**< Successfully analyzed tracks */
    int      good_tracks;                       /**< Tracks with quality >= 90 */
    int      fair_tracks;                       /**< Tracks with quality 70-89 */
    int      poor_tracks;                       /**< Tracks with quality 50-69 */
    int      bad_tracks;                        /**< Tracks with quality < 50 */
    int      total_anomalies;                   /**< Total anomaly count */
    double   avg_quality;                       /**< Average quality score */
    double   min_quality;                       /**< Minimum quality score */
    double   max_quality;                       /**< Maximum quality score */
    double   avg_flux;                          /**< Average flux count */
    int      worst_track;                       /**< Track with lowest quality */
    int      worst_side;                        /**< Side of worst track */
    int      best_track;                        /**< Track with highest quality */
    int      best_side;                         /**< Side of best track */
    char     disk_grade;                        /**< Overall grade A-F */
} uft_surface_stats_t;

/*===========================================================================
 * Alignment Status Enumeration
 *===========================================================================*/

typedef enum {
    UFT_ALIGN_UNKNOWN = 0,
    UFT_ALIGN_GOOD,
    UFT_ALIGN_FAIR,
    UFT_ALIGN_POOR,
    UFT_ALIGN_BAD
} uft_alignment_status_t;

/*===========================================================================
 * Alignment Analysis Result
 *===========================================================================*/

typedef struct {
    bool     alignment_ok;                      /**< Alignment within tolerance */
    uft_alignment_status_t alignment_status;    /**< Alignment status enum */
    double   azimuth_error;                     /**< Azimuth error estimate */
    double   radial_error;                      /**< Radial error estimate */
    double   radial_gradient;                   /**< Radial flux gradient */
    double   track_pitch_error;                 /**< Track pitch deviation */
    double   flux_variance;                     /**< Flux count variance */
    double   mean_flux;                         /**< Mean flux count */
    int      problem_track;                     /**< Track showing alignment issue */
    int      problem_side;                      /**< Side showing alignment issue */
    double   confidence;                        /**< Analysis confidence 0-1 */
} uft_alignment_result_t;

/*===========================================================================
 * Defect Entry
 *===========================================================================*/

typedef struct {
    int      track;                             /**< Track number */
    int      side;                              /**< Side number */
    uft_defect_type_t type;                     /**< Defect type */
    int      severity;                          /**< Severity 0-100 */
    size_t   position;                          /**< Position in flux stream */
} uft_defect_t;

/*===========================================================================
 * Legacy Type Alias
 *===========================================================================*/

typedef uft_surface_stats_t uft_surface_result_t;

/*===========================================================================
 * Function Prototypes
 *===========================================================================*/

/**
 * @brief Analyze a single track's surface
 */
int uft_surface_analyze_track(const uint32_t *flux_times, size_t flux_count,
                              double sample_clock,
                              uft_track_surface_t *surface);

/**
 * @brief Initialize a surface map
 */
int uft_surface_map_init(uft_surface_map_t *map, int tracks, int sides);

/**
 * @brief Free surface map resources
 */
void uft_surface_map_free(uft_surface_map_t *map);

/**
 * @brief Set track data in surface map
 */
int uft_surface_map_set_track(uft_surface_map_t *map, int track, int side,
                              const uft_track_surface_t *surface);

/**
 * @brief Get track data from surface map
 */
const uft_track_surface_t *uft_surface_map_get_track(const uft_surface_map_t *map,
                                                     int track, int side);

/**
 * @brief Get surface statistics
 */
int uft_surface_get_stats(const uft_surface_map_t *map, 
                          uft_surface_stats_t *stats);

/**
 * @brief Check head alignment
 */
int uft_surface_check_alignment(const uft_surface_map_t *map,
                                uft_alignment_result_t *result);

/**
 * @brief Find defects on disk surface
 */
int uft_surface_find_defects(const uft_surface_map_t *map,
                             uft_defect_t *defects, size_t max_defects,
                             size_t *defect_count);

/**
 * @brief Generate JSON report
 */
int uft_surface_report_json(const uft_surface_stats_t *stats,
                            char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SURFACE_ANALYSIS_H */
