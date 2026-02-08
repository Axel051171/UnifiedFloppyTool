/**
 * @file uft_event_v8_bridge.h
 * @brief UFT ↔ OTDR Event v8 Bridge (Multi-Scale + Pass/Fail)
 *
 * Extends the v2 event bridge with v8 capabilities:
 *   - Multi-scale detection (fine spikes + broad degradation)
 *   - Polarity pattern analysis
 *   - Extended event taxonomy (8 event types)
 *   - Smart RL merge with configurable rules
 *   - Per-segment Pass/Fail/Warn verdicts
 *
 * UFT domain mapping (extends v2):
 *   OTDR v8                →  UFT floppy domain
 *   ───────────────────       ──────────────────────────
 *   REFLECTION             →  Timing spike
 *   ATTENUATION            →  Signal degradation
 *   REFLECT_LOSS           →  Compound anomaly (connector)
 *   GAINUP                 →  Signal recovery (head position change)
 *   SPIKE_NEG              →  Dropout glitch
 *   OSCILLATION            →  Head ringing / flutter
 *   BROADLOSS              →  Gradual media wear / weak zone
 */

#ifndef UFT_EVENT_V8_BRIDGE_H
#define UFT_EVENT_V8_BRIDGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════
 * Extended event types (superset of v2 bridge)
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_EV8_NORMAL      = 0,
    UFT_EV8_SPIKE       = 1,
    UFT_EV8_DEGRADATION = 2,
    UFT_EV8_COMPOUND    = 3,
    UFT_EV8_RECOVERY    = 4,   /**< Signal gain/recovery */
    UFT_EV8_DROPOUT     = 5,   /**< Negative spike / dropout glitch */
    UFT_EV8_FLUTTER     = 6,   /**< Oscillation / head ringing */
    UFT_EV8_WEAKSIGNAL  = 7    /**< Broad gradual degradation */
} uft_ev8_type_t;

typedef enum {
    UFT_EV8_PASS = 0,
    UFT_EV8_WARN = 1,
    UFT_EV8_FAIL = 2
} uft_ev8_verdict_t;

/* Fail reason bitmask */
enum {
    UFT_EV8_REASON_NONE       = 0u,
    UFT_EV8_REASON_HIGH_LOSS  = 1u << 0,
    UFT_EV8_REASON_LOW_SNR    = 1u << 1,
    UFT_EV8_REASON_TOO_LONG   = 1u << 2,
    UFT_EV8_REASON_HIGH_REFL  = 1u << 3,
    UFT_EV8_REASON_PATTERN    = 1u << 4
};

/* ═══════════════════════════════════════════════════════════════════
 * Event info (per detected event)
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    uft_ev8_type_t    type;
    size_t            start;
    size_t            end;
    size_t            length;
    float             confidence;
    float             severity;
    float             peak_snr_db;
    float             peak_amplitude;
    uint8_t           dominant_scale;    /**< Which scale detected it best */
    bool              is_merged;

    uft_ev8_verdict_t verdict;
    uint32_t          fail_reasons;
} uft_ev8_event_t;

/* ═══════════════════════════════════════════════════════════════════
 * Configuration
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Scale windows (up to 8) */
    size_t  scale_windows[8];
    size_t  num_scales;

    /* Thresholds */
    float   spike_snr_db;
    float   degrad_snr_db;
    float   dropout_snr_db;
    float   flutter_snr_db;
    float   broadloss_snr_db;
    float   min_signal_rms;

    /* Noise */
    bool    local_sigma;
    size_t  sigma_window;
    size_t  sigma_stride;

    /* Merge */
    bool    enable_merge;
    bool    iterative_merge;

    /* Pass/fail */
    bool    enable_passfail;
    float   pf_max_loss_db;
    float   pf_max_reflectance_db;
    float   pf_min_snr_db;
    size_t  pf_max_event_length;
    float   pf_warn_factor;

    /* Filtering */
    size_t  min_event_len;
    float   min_confidence;
} uft_ev8_config_t;

/* ═══════════════════════════════════════════════════════════════════
 * Report
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    size_t  total_events;
    size_t  spike_count;
    size_t  degradation_count;
    size_t  compound_count;
    size_t  recovery_count;
    size_t  dropout_count;
    size_t  flutter_count;
    size_t  weaksignal_count;

    /* Verdicts */
    size_t  pass_count;
    size_t  warn_count;
    size_t  fail_count;

    /* Quality */
    float   event_density;
    float   affected_fraction;
    float   quality_score;
    float   mean_snr_db;
    float   sigma_mean;

    size_t  samples_analyzed;
} uft_ev8_report_t;

/* ═══════════════════════════════════════════════════════════════════
 * Context
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    uft_ev8_config_t  cfg;
    uft_ev8_report_t  report;

    uft_ev8_event_t  *events;
    size_t            event_count;
    size_t            event_capacity;

    bool initialized;
} uft_ev8_ctx_t;

/* ═══════════════════════════════════════════════════════════════════
 * Error codes
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_EV8_OK           =  0,
    UFT_EV8_ERR_NULL     = -1,
    UFT_EV8_ERR_NOMEM    = -2,
    UFT_EV8_ERR_SMALL    = -3,
    UFT_EV8_ERR_CONFIG   = -4,
    UFT_EV8_ERR_INTERNAL = -5
} uft_ev8_error_t;

/* ═══════════════════════════════════════════════════════════════════
 * API
 * ═══════════════════════════════════════════════════════════════════ */

uft_ev8_config_t  uft_ev8_default_config(void);
uft_ev8_error_t   uft_ev8_init(uft_ev8_ctx_t *ctx, const uft_ev8_config_t *cfg);
void              uft_ev8_free(uft_ev8_ctx_t *ctx);

uft_ev8_error_t   uft_ev8_detect_float(uft_ev8_ctx_t *ctx, const float *signal, size_t n);
uft_ev8_error_t   uft_ev8_detect_flux_ns(uft_ev8_ctx_t *ctx, const uint32_t *flux, size_t n);
uft_ev8_error_t   uft_ev8_detect_analog(uft_ev8_ctx_t *ctx, const int16_t *samples, size_t n);

size_t                  uft_ev8_count(const uft_ev8_ctx_t *ctx);
const uft_ev8_event_t * uft_ev8_get(const uft_ev8_ctx_t *ctx, size_t idx);
uft_ev8_report_t        uft_ev8_get_report(const uft_ev8_ctx_t *ctx);

/* Count events by verdict */
size_t uft_ev8_count_by_verdict(const uft_ev8_ctx_t *ctx, uft_ev8_verdict_t v);

const char *uft_ev8_type_str(uft_ev8_type_t t);
const char *uft_ev8_verdict_str(uft_ev8_verdict_t v);
const char *uft_ev8_error_str(uft_ev8_error_t e);
const char *uft_ev8_version(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_EVENT_V8_BRIDGE_H */
