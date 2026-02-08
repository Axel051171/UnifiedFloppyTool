/**
 * @file uft_event_bridge.h
 * @brief UFT ↔ OTDR Event Detection Bridge
 *
 * Connects the OTDR Event Core v2 (event detection, classification,
 * segment merging) to UFT's flux analysis pipeline. Maps fiber-optic
 * OTDR terminology to floppy disk domain:
 *
 *   OTDR fiber domain       →  UFT floppy domain
 *   ─────────────────────      ──────────────────
 *   REFLECTION (spike)      →  Timing spike (bad sector, copy protection)
 *   ATTENUATION (step-down) →  Signal degradation (media wear, weak bits)
 *   REFLECT_LOSS (merged)   →  Compound anomaly (damaged region)
 *
 * Pipeline position:
 *   raw flux → [denoise] → event detection → segment analysis → quality score
 *
 * Can chain with uft_denoise_bridge for pre-filtering.
 */

#ifndef UFT_EVENT_BRIDGE_H
#define UFT_EVENT_BRIDGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════
 * Domain-mapped event types
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_EVT_NORMAL       = 0,  /**< Normal flux region */
    UFT_EVT_SPIKE        = 1,  /**< Timing spike (Fresnel-like reflection) */
    UFT_EVT_DEGRADATION  = 2,  /**< Signal degradation (attenuation step) */
    UFT_EVT_COMPOUND     = 3,  /**< Compound anomaly (spike + degradation merged) */
    UFT_EVT_WEAK_ZONE    = 4   /**< Extended weak signal zone (low SNR) */
} uft_event_type_t;

/** Single detected event (segment) */
typedef struct {
    uft_event_type_t type;
    size_t  start;         /**< First sample index (inclusive) */
    size_t  end;           /**< Last sample index (inclusive) */
    size_t  length;        /**< Number of samples */
    float   confidence;    /**< Detection confidence 0.0–1.0 */
    float   severity;      /**< Severity metric 0.0–1.0 (amplitude of anomaly) */
    float   snr_mean_db;   /**< Mean SNR in event region */
    float   amplitude;     /**< Peak |Δ| in event region */
    bool    is_merged;     /**< True if this event was merged from spike+step */
} uft_event_info_t;

/* ═══════════════════════════════════════════════════════════════════
 * Configuration
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Detection thresholds */
    float   spike_snr_db;       /**< SNR threshold for spike detection (default 12) */
    float   degrad_snr_db;      /**< SNR threshold for degradation (default 10) */
    float   min_signal_rms;     /**< Minimum signal RMS for detection (default 1e-4) */

    /* Local noise estimation */
    bool    local_sigma;        /**< Use local sigma estimation (default true) */
    size_t  sigma_window;       /**< Window for local MAD sigma (default 4096) */
    size_t  sigma_stride;       /**< Recompute interval (default 256) */

    /* Envelope */
    size_t  env_window;         /**< RMS envelope window (default 1025) */

    /* Segment merge */
    bool    enable_merge;       /**< Merge spike+step events (default true) */
    size_t  merge_gap;          /**< Max gap for merging (default 64 samples) */
    size_t  min_spike_len;      /**< Min samples for spike segment (default 1) */
    size_t  min_degrad_len;     /**< Min samples for degradation segment (default 2) */

    /* Filtering */
    size_t  min_event_len;      /**< Discard events shorter than this (default 1) */
    float   min_confidence;     /**< Discard events below this confidence (default 0) */
} uft_event_config_t;

/* ═══════════════════════════════════════════════════════════════════
 * Results
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Summary */
    size_t  total_events;       /**< Total events detected */
    size_t  spike_count;        /**< Spike events */
    size_t  degradation_count;  /**< Degradation events */
    size_t  compound_count;     /**< Compound events */
    size_t  weak_zone_count;    /**< Weak zone events */

    /* Quality metrics */
    float   event_density;      /**< Events per 1000 samples */
    float   affected_fraction;  /**< Fraction of signal affected by events */
    float   mean_snr_db;        /**< Global mean SNR of entire signal */
    float   worst_snr_db;       /**< Worst (lowest) SNR in any event */
    float   quality_score;      /**< Overall quality 0.0–1.0 (1=perfect) */

    /* Noise */
    float   sigma_mean;         /**< Mean noise sigma across signal */
    float   sigma_max;          /**< Maximum local noise sigma */

    size_t  samples_analyzed;   /**< Total samples processed */
} uft_event_report_t;

/* ═══════════════════════════════════════════════════════════════════
 * Context
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    uft_event_config_t cfg;
    uft_event_report_t report;

    /* Internal storage for events */
    uft_event_info_t *events;
    size_t event_count;
    size_t event_capacity;

    bool initialized;
} uft_event_ctx_t;

/* ═══════════════════════════════════════════════════════════════════
 * Error codes
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_EVT_OK           =  0,
    UFT_EVT_ERR_NULL     = -1,
    UFT_EVT_ERR_NOMEM    = -2,
    UFT_EVT_ERR_SMALL    = -3,
    UFT_EVT_ERR_CONFIG   = -4,
    UFT_EVT_ERR_INTERNAL = -5
} uft_event_error_t;

/* ═══════════════════════════════════════════════════════════════════
 * API: Init / Free
 * ═══════════════════════════════════════════════════════════════════ */

uft_event_config_t uft_event_default_config(void);

uft_event_error_t uft_event_init(uft_event_ctx_t *ctx,
                                  const uft_event_config_t *cfg);

void uft_event_free(uft_event_ctx_t *ctx);

/* ═══════════════════════════════════════════════════════════════════
 * API: Detection
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * Detect events in a float signal (amplitude trace).
 * This is the main entry point for arbitrary 1D signals.
 */
uft_event_error_t uft_event_detect_float(uft_event_ctx_t *ctx,
                                           const float *signal,
                                           size_t count);

/**
 * Detect events in raw flux intervals (uint32_t nanoseconds).
 * Converts to float internally.
 */
uft_event_error_t uft_event_detect_flux_ns(uft_event_ctx_t *ctx,
                                             const uint32_t *flux_ns,
                                             size_t count);

/**
 * Detect events in analog samples (int16_t).
 * Normalizes to float internally.
 */
uft_event_error_t uft_event_detect_analog(uft_event_ctx_t *ctx,
                                            const int16_t *samples,
                                            size_t count);

/* ═══════════════════════════════════════════════════════════════════
 * API: Results
 * ═══════════════════════════════════════════════════════════════════ */

/** Get number of detected events */
size_t uft_event_count(const uft_event_ctx_t *ctx);

/** Get event by index (NULL if out of range) */
const uft_event_info_t *uft_event_get(const uft_event_ctx_t *ctx, size_t idx);

/** Get summary report */
uft_event_report_t uft_event_get_report(const uft_event_ctx_t *ctx);

/** Event type name string */
const char *uft_event_type_str(uft_event_type_t type);

/** Error string */
const char *uft_event_error_str(uft_event_error_t err);

/** Module version */
const char *uft_event_version(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_EVENT_BRIDGE_H */
