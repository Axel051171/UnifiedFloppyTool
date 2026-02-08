/**
 * @file uft_crosstalk_filter.h
 * @brief Crosstalk Filter for Floppy Disk Writing
 * 
 * Based on DTC learnings - implements per-side crosstalk filtering
 * to prevent interference between adjacent tracks during writing.
 * 
 * Crosstalk occurs when magnetic flux from one track "bleeds" into
 * adjacent tracks. This filter detects and compensates for such effects.
 * 
 * CLEAN-ROOM implementation based on observable requirements.
 */

#ifndef UFT_CROSSTALK_FILTER_H
#define UFT_CROSSTALK_FILTER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants (based on DTC observations)
 * ============================================================================ */

/** Side selection flags (like DTC -wk) */
#define UFT_CT_SIDE_NONE    0
#define UFT_CT_SIDE_0       1
#define UFT_CT_SIDE_1       2
#define UFT_CT_SIDE_BOTH    3

/** Filter modes */
#define UFT_CT_MODE_OFF         0
#define UFT_CT_MODE_DETECT      1   /**< Detect only, don't filter */
#define UFT_CT_MODE_FILTER      2   /**< Detect and filter */
#define UFT_CT_MODE_AGGRESSIVE  3   /**< Aggressive filtering */

/** Detection thresholds */
#define UFT_CT_THRESHOLD_LOW    0.10  /**< 10% amplitude difference */
#define UFT_CT_THRESHOLD_MED    0.20  /**< 20% amplitude difference */
#define UFT_CT_THRESHOLD_HIGH   0.30  /**< 30% amplitude difference */

/* ============================================================================
 * Data Types
 * ============================================================================ */

/**
 * @brief Crosstalk filter configuration
 */
typedef struct {
    uint8_t  sides_enabled;       /**< Which sides to filter (UFT_CT_SIDE_*) */
    uint8_t  mode;                /**< Filter mode */
    double   threshold;           /**< Detection threshold (0.0-1.0) */
    bool     enabled;             /**< Master enable */
    
    /* Advanced options */
    uint8_t  window_tracks;       /**< Tracks to consider (default 1) */
    double   amplitude_weight;    /**< Weight for amplitude analysis */
    double   phase_weight;        /**< Weight for phase analysis */
    bool     adaptive;            /**< Adapt threshold per track */
} uft_crosstalk_config_t;

/**
 * @brief Crosstalk detection result for a single position
 */
typedef struct {
    uint64_t position;            /**< Bit/sample position */
    double   crosstalk_level;     /**< Detected level (0.0-1.0) */
    int8_t   source_track_delta;  /**< Suspected source track offset */
    bool     is_crosstalk;        /**< True if crosstalk detected */
    bool     was_filtered;        /**< True if filtering was applied */
} uft_crosstalk_point_t;

/**
 * @brief Crosstalk analysis result for a track
 */
typedef struct {
    uint8_t  track;
    uint8_t  head;
    
    /* Detection results */
    uint32_t points_analyzed;
    uint32_t points_detected;     /**< Points with crosstalk */
    uint32_t points_filtered;     /**< Points that were filtered */
    
    /* Statistics */
    double   max_crosstalk_level;
    double   avg_crosstalk_level;
    double   crosstalk_percentage; /**< % of track affected */
    
    /* Source analysis */
    int32_t  primary_source_delta; /**< Most common source track offset */
    
    /* Quality assessment */
    uint8_t  quality_before;      /**< Quality score before filtering */
    uint8_t  quality_after;       /**< Quality score after filtering */
} uft_crosstalk_result_t;

/**
 * @brief Crosstalk filter state
 */
typedef struct {
    uft_crosstalk_config_t config;
    
    /* Reference data from adjacent tracks */
    uint8_t *ref_track_minus;     /**< Data from track-1 */
    uint8_t *ref_track_plus;      /**< Data from track+1 */
    size_t   ref_size;
    
    /* Current track being processed */
    uint8_t  current_track;
    uint8_t  current_head;
    
    /* Running statistics */
    uint64_t total_analyzed;
    uint64_t total_detected;
    uint64_t total_filtered;
} uft_crosstalk_state_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Initialize crosstalk filter config with defaults
 */
void uft_crosstalk_config_init(uft_crosstalk_config_t *config);

/**
 * @brief Initialize crosstalk filter state
 * 
 * @param state   State to initialize
 * @param config  Configuration to use
 */
void uft_crosstalk_state_init(
    uft_crosstalk_state_t *state,
    const uft_crosstalk_config_t *config
);

/**
 * @brief Free crosstalk filter state resources
 */
void uft_crosstalk_state_free(uft_crosstalk_state_t *state);

/**
 * @brief Set reference data from adjacent tracks
 * 
 * @param state           Filter state
 * @param track_minus     Data from track-1 (can be NULL)
 * @param track_plus      Data from track+1 (can be NULL)
 * @param size            Size of reference data
 */
void uft_crosstalk_set_reference(
    uft_crosstalk_state_t *state,
    const uint8_t *track_minus,
    const uint8_t *track_plus,
    size_t size
);

/**
 * @brief Set current track being processed
 */
void uft_crosstalk_set_track(
    uft_crosstalk_state_t *state,
    uint8_t track,
    uint8_t head
);

/**
 * @brief Detect crosstalk in track data
 * 
 * @param state           Filter state
 * @param track_data      Track data to analyze
 * @param data_size       Size of track data
 * @param result          Output analysis result
 * @return 0 on success
 */
int uft_crosstalk_detect(
    uft_crosstalk_state_t *state,
    const uint8_t *track_data,
    size_t data_size,
    uft_crosstalk_result_t *result
);

/**
 * @brief Filter crosstalk from track data
 * 
 * @param state           Filter state
 * @param track_data      Track data to filter (modified in place)
 * @param data_size       Size of track data
 * @param result          Output analysis result
 * @return Number of points filtered
 */
size_t uft_crosstalk_filter(
    uft_crosstalk_state_t *state,
    uint8_t *track_data,
    size_t data_size,
    uft_crosstalk_result_t *result
);

/**
 * @brief Detect and filter in one pass
 * 
 * @param state           Filter state
 * @param track_data      Track data (modified if filtering enabled)
 * @param data_size       Size of track data
 * @param result          Output analysis result
 * @return 0 on success
 */
int uft_crosstalk_process(
    uft_crosstalk_state_t *state,
    uint8_t *track_data,
    size_t data_size,
    uft_crosstalk_result_t *result
);

/* ============================================================================
 * Flux-Level Functions
 * ============================================================================ */

/**
 * @brief Detect crosstalk in flux timing data
 * 
 * @param state           Filter state
 * @param flux_data       Flux timing data
 * @param flux_count      Number of flux transitions
 * @param ref_minus       Reference flux from track-1 (can be NULL)
 * @param ref_plus        Reference flux from track+1 (can be NULL)
 * @param ref_count       Reference data count
 * @param result          Output result
 * @return 0 on success
 */
int uft_crosstalk_detect_flux(
    uft_crosstalk_state_t *state,
    const uint32_t *flux_data,
    size_t flux_count,
    const uint32_t *ref_minus,
    const uint32_t *ref_plus,
    size_t ref_count,
    uft_crosstalk_result_t *result
);

/**
 * @brief Filter crosstalk from flux timing data
 * 
 * @param state           Filter state
 * @param flux_data       Flux data (modified in place)
 * @param flux_count      Number of flux transitions
 * @param result          Output result
 * @return Number of transitions filtered
 */
size_t uft_crosstalk_filter_flux(
    uft_crosstalk_state_t *state,
    uint32_t *flux_data,
    size_t flux_count,
    uft_crosstalk_result_t *result
);

/* ============================================================================
 * Analysis Functions
 * ============================================================================ */

/**
 * @brief Calculate correlation between two data streams
 * 
 * @param data1    First data stream
 * @param data2    Second data stream
 * @param size     Size of data
 * @param offset   Offset to apply to data2
 * @return Correlation coefficient (-1.0 to 1.0)
 */
double uft_crosstalk_correlate(
    const uint8_t *data1,
    const uint8_t *data2,
    size_t size,
    int offset
);

/**
 * @brief Find best correlation offset
 * 
 * @param data1        First data stream
 * @param data2        Second data stream
 * @param size         Size of data
 * @param max_offset   Maximum offset to search
 * @param best_corr    Output: best correlation found
 * @return Best offset value
 */
int uft_crosstalk_find_offset(
    const uint8_t *data1,
    const uint8_t *data2,
    size_t size,
    int max_offset,
    double *best_corr
);

/**
 * @brief Estimate crosstalk level at a position
 * 
 * @param track_data     Current track data
 * @param ref_data       Reference track data
 * @param position       Position to analyze
 * @param window         Analysis window size
 * @return Estimated crosstalk level (0.0-1.0)
 */
double uft_crosstalk_estimate_level(
    const uint8_t *track_data,
    const uint8_t *ref_data,
    size_t position,
    size_t window
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get crosstalk statistics
 */
void uft_crosstalk_get_stats(
    const uft_crosstalk_state_t *state,
    uint64_t *analyzed,
    uint64_t *detected,
    uint64_t *filtered
);

/**
 * @brief Reset crosstalk state statistics
 */
void uft_crosstalk_reset_stats(uft_crosstalk_state_t *state);

/**
 * @brief Convert result to JSON
 */
size_t uft_crosstalk_result_to_json(
    const uft_crosstalk_result_t *result,
    char *buffer,
    size_t size
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CROSSTALK_FILTER_H */
