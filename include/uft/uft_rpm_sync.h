/**
 * @file uft_rpm_sync.h
 * @brief RPM Measurement and Drive Synchronization
 * 
 * Inspired by BLITZ Atari ST copy system which required
 * drive RPM matching within 0.09 RPM for streaming copy.
 * 
 * Features:
 * - RPM measurement from index pulses
 * - Drive synchronization detection
 * - Sync loss detection during operations
 * - RPM drift monitoring
 * 
 * @version 1.0.0
 */

#ifndef UFT_RPM_SYNC_H
#define UFT_RPM_SYNC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

/** Standard RPM values */
#define UFT_RPM_STANDARD_DD     300.0   /**< DD/HD 3.5"/5.25" */
#define UFT_RPM_STANDARD_HD     360.0   /**< 8" and some 5.25" HD */
#define UFT_RPM_STANDARD_C64    300.0   /**< Commodore 1541 */
#define UFT_RPM_STANDARD_APPLE  300.0   /**< Apple II (variable) */

/** Tolerance values */
#define UFT_RPM_TOLERANCE_STRICT    0.09    /**< BLITZ requirement */
#define UFT_RPM_TOLERANCE_NORMAL    0.30    /**< Normal operation */
#define UFT_RPM_TOLERANCE_RELAXED   1.00    /**< Relaxed for damaged disks */

/** Sync loss detection */
#define UFT_SYNC_TIMEOUT_NS     1000000000  /**< 1 second */
#define UFT_SYNC_TIMEOUT_MS     1000

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * @brief RPM measurement result
 */
typedef struct {
    double rpm_measured;        /**< Measured RPM */
    double rpm_nominal;         /**< Expected RPM (300/360) */
    double drift_percent;       /**< Drift as percentage */
    double period_ms;           /**< Revolution period in ms */
    uint32_t sample_count;      /**< Number of revolutions measured */
    bool in_spec;               /**< Within nominal tolerance */
    bool stable;                /**< RPM stable over time */
} uft_rpm_status_t;

/**
 * @brief Drive synchronization status
 */
typedef struct {
    /* Drive A (source) */
    double rpm_a;
    bool rpm_a_valid;
    
    /* Drive B (destination) */
    double rpm_b;
    bool rpm_b_valid;
    
    /* Sync status */
    double rpm_difference;      /**< Absolute difference */
    double tolerance;           /**< Current tolerance setting */
    bool synced;                /**< Drives are synchronized */
    
    /* Streaming status */
    bool streaming_ok;          /**< OK for streaming copy */
    const char *warning;        /**< Warning message (NULL if none) */
} uft_drive_sync_t;

/**
 * @brief Sync loss detector
 */
typedef struct {
    uint64_t last_activity_ns;  /**< Last activity timestamp */
    uint64_t timeout_ns;        /**< Timeout threshold */
    uint64_t start_ns;          /**< Operation start time */
    uint32_t stall_count;       /**< Number of stalls detected */
    bool sync_lost;             /**< Sync has been lost */
    bool timeout_occurred;      /**< Timeout has occurred */
} uft_sync_detector_t;

/**
 * @brief RPM history for drift detection
 */
typedef struct {
    double samples[32];         /**< RPM samples */
    size_t sample_count;        /**< Number of samples */
    size_t sample_index;        /**< Current index (circular) */
    double min_rpm;             /**< Minimum observed */
    double max_rpm;             /**< Maximum observed */
    double avg_rpm;             /**< Average RPM */
    double variance;            /**< Variance */
} uft_rpm_history_t;

/*============================================================================
 * RPM Measurement
 *============================================================================*/

/**
 * @brief Measure RPM from index pulse timestamps
 * 
 * @param index_times Array of index pulse timestamps (ns)
 * @param count Number of timestamps (min 2)
 * @param nominal Expected RPM (300 or 360)
 * @param out Output status
 * @return 0 on success, -1 on error
 */
static inline int uft_rpm_measure(const uint64_t *index_times,
                                   size_t count,
                                   double nominal,
                                   uft_rpm_status_t *out)
{
    if (!index_times || count < 2 || !out) return -1;
    
    /* Calculate average period */
    uint64_t total_ns = 0;
    for (size_t i = 1; i < count; i++) {
        if (index_times[i] > index_times[i-1]) {
            total_ns += index_times[i] - index_times[i-1];
        }
    }
    
    if (total_ns == 0) return -1;
    
    double avg_period_ns = (double)total_ns / (double)(count - 1);
    double period_ms = avg_period_ns / 1000000.0;
    double rpm = 60000.0 / period_ms;  /* 60s / period_ms */
    
    out->rpm_measured = rpm;
    out->rpm_nominal = nominal;
    out->period_ms = period_ms;
    out->sample_count = (uint32_t)(count - 1);
    out->drift_percent = ((rpm - nominal) / nominal) * 100.0;
    out->in_spec = (out->drift_percent >= -0.3 && out->drift_percent <= 0.3);
    out->stable = true;  /* TODO: Calculate variance */
    
    return 0;
}

/**
 * @brief Estimate RPM from flux track data
 * 
 * Uses index markers in flux data if available.
 */
static inline double uft_rpm_estimate_from_track_length(size_t flux_samples,
                                                         double sample_rate_mhz,
                                                         double nominal_rpm)
{
    /* Calculate expected samples per revolution */
    double period_us = 60000000.0 / nominal_rpm;  /* µs per rev */
    double expected_samples = period_us * sample_rate_mhz;
    
    /* Estimate actual RPM */
    double actual_period_us = (double)flux_samples / sample_rate_mhz;
    double actual_rpm = 60000000.0 / actual_period_us;
    
    return actual_rpm;
}

/*============================================================================
 * Drive Synchronization
 *============================================================================*/

/**
 * @brief Check if two drives are synchronized
 * 
 * @param rpm_a Drive A RPM
 * @param rpm_b Drive B RPM
 * @param tolerance Maximum allowed difference
 * @return true if synchronized
 */
static inline bool uft_rpm_drives_synced(double rpm_a, 
                                          double rpm_b, 
                                          double tolerance)
{
    double diff = rpm_a - rpm_b;
    if (diff < 0) diff = -diff;
    return diff <= tolerance;
}

/**
 * @brief Initialize drive sync status
 */
static inline void uft_drive_sync_init(uft_drive_sync_t *sync)
{
    sync->rpm_a = 0;
    sync->rpm_b = 0;
    sync->rpm_a_valid = false;
    sync->rpm_b_valid = false;
    sync->rpm_difference = 0;
    sync->tolerance = UFT_RPM_TOLERANCE_STRICT;
    sync->synced = false;
    sync->streaming_ok = false;
    sync->warning = NULL;
}

/**
 * @brief Update drive sync status
 * 
 * @param sync Sync status structure
 * @param rpm_a Drive A RPM (or 0 if unknown)
 * @param rpm_b Drive B RPM (or 0 if unknown)
 */
static inline void uft_drive_sync_update(uft_drive_sync_t *sync,
                                          double rpm_a,
                                          double rpm_b)
{
    sync->rpm_a = rpm_a;
    sync->rpm_b = rpm_b;
    sync->rpm_a_valid = (rpm_a > 0);
    sync->rpm_b_valid = (rpm_b > 0);
    
    if (sync->rpm_a_valid && sync->rpm_b_valid) {
        sync->rpm_difference = rpm_a - rpm_b;
        if (sync->rpm_difference < 0) {
            sync->rpm_difference = -sync->rpm_difference;
        }
        
        sync->synced = (sync->rpm_difference <= sync->tolerance);
        sync->streaming_ok = sync->synced;
        
        if (!sync->synced) {
            sync->warning = "Drive RPM mismatch - streaming copy may fail";
        } else if (sync->rpm_difference > UFT_RPM_TOLERANCE_STRICT) {
            sync->warning = "RPM difference detected - using normal mode";
        } else {
            sync->warning = NULL;
        }
    } else {
        sync->synced = false;
        sync->streaming_ok = false;
        sync->warning = "Unable to measure drive RPM";
    }
}

/*============================================================================
 * Sync Loss Detection (BLITZ Style)
 *============================================================================*/

/**
 * @brief Initialize sync loss detector
 * 
 * @param det Detector structure
 * @param timeout_ns Timeout in nanoseconds (default: 1s)
 */
static inline void uft_sync_detector_init(uft_sync_detector_t *det,
                                           uint64_t timeout_ns)
{
    det->last_activity_ns = 0;
    det->timeout_ns = timeout_ns ? timeout_ns : UFT_SYNC_TIMEOUT_NS;
    det->start_ns = 0;
    det->stall_count = 0;
    det->sync_lost = false;
    det->timeout_occurred = false;
}

/**
 * @brief Start sync monitoring
 * 
 * @param det Detector
 * @param now_ns Current timestamp
 */
static inline void uft_sync_detector_start(uft_sync_detector_t *det,
                                            uint64_t now_ns)
{
    det->start_ns = now_ns;
    det->last_activity_ns = now_ns;
    det->stall_count = 0;
    det->sync_lost = false;
    det->timeout_occurred = false;
}

/**
 * @brief Update activity timestamp
 * 
 * Call this whenever there is activity (data received, track complete, etc.)
 * 
 * @param det Detector
 * @param now_ns Current timestamp
 */
static inline void uft_sync_detector_activity(uft_sync_detector_t *det,
                                               uint64_t now_ns)
{
    det->last_activity_ns = now_ns;
}

/**
 * @brief Check for sync loss
 * 
 * @param det Detector
 * @param now_ns Current timestamp
 * @return true if sync has been lost
 */
static inline bool uft_sync_detector_check(uft_sync_detector_t *det,
                                            uint64_t now_ns)
{
    if (det->sync_lost) return true;
    
    if (det->last_activity_ns == 0) {
        det->last_activity_ns = now_ns;
        return false;
    }
    
    uint64_t elapsed = now_ns - det->last_activity_ns;
    
    if (elapsed >= det->timeout_ns) {
        det->sync_lost = true;
        det->timeout_occurred = true;
        det->stall_count++;
        return true;
    }
    
    /* Warn if approaching timeout */
    if (elapsed >= det->timeout_ns / 2) {
        det->stall_count++;
    }
    
    return false;
}

/**
 * @brief Reset detector after stall recovery
 * 
 * @param det Detector
 * @param now_ns Current timestamp
 */
static inline void uft_sync_detector_reset(uft_sync_detector_t *det,
                                            uint64_t now_ns)
{
    det->last_activity_ns = now_ns;
    det->sync_lost = false;
    det->timeout_occurred = false;
}

/*============================================================================
 * RPM History Tracking
 *============================================================================*/

/**
 * @brief Initialize RPM history tracker
 */
static inline void uft_rpm_history_init(uft_rpm_history_t *hist)
{
    hist->sample_count = 0;
    hist->sample_index = 0;
    hist->min_rpm = 1000.0;
    hist->max_rpm = 0.0;
    hist->avg_rpm = 0.0;
    hist->variance = 0.0;
}

/**
 * @brief Add RPM sample to history
 */
static inline void uft_rpm_history_add(uft_rpm_history_t *hist, double rpm)
{
    /* Circular buffer */
    hist->samples[hist->sample_index] = rpm;
    hist->sample_index = (hist->sample_index + 1) % 32;
    if (hist->sample_count < 32) hist->sample_count++;
    
    /* Update min/max */
    if (rpm < hist->min_rpm) hist->min_rpm = rpm;
    if (rpm > hist->max_rpm) hist->max_rpm = rpm;
    
    /* Update average */
    double sum = 0;
    for (size_t i = 0; i < hist->sample_count; i++) {
        sum += hist->samples[i];
    }
    hist->avg_rpm = sum / (double)hist->sample_count;
    
    /* Update variance */
    double var_sum = 0;
    for (size_t i = 0; i < hist->sample_count; i++) {
        double diff = hist->samples[i] - hist->avg_rpm;
        var_sum += diff * diff;
    }
    hist->variance = var_sum / (double)hist->sample_count;
}

/**
 * @brief Check if RPM is stable
 * 
 * @param hist History tracker
 * @param tolerance Max acceptable variance
 * @return true if RPM is stable
 */
static inline bool uft_rpm_history_is_stable(const uft_rpm_history_t *hist,
                                              double tolerance)
{
    if (hist->sample_count < 4) return false;
    return (hist->max_rpm - hist->min_rpm) <= tolerance;
}

/**
 * @brief Get RPM drift over time
 * 
 * @param hist History tracker
 * @return Drift (positive = speeding up, negative = slowing down)
 */
static inline double uft_rpm_history_drift(const uft_rpm_history_t *hist)
{
    if (hist->sample_count < 4) return 0.0;
    
    /* Compare first half average to second half */
    size_t half = hist->sample_count / 2;
    double first_avg = 0, second_avg = 0;
    
    for (size_t i = 0; i < half; i++) {
        first_avg += hist->samples[i];
    }
    first_avg /= (double)half;
    
    for (size_t i = half; i < hist->sample_count; i++) {
        second_avg += hist->samples[i];
    }
    second_avg /= (double)(hist->sample_count - half);
    
    return second_avg - first_avg;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_RPM_SYNC_H */
