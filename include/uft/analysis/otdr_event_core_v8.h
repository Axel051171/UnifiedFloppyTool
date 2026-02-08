#ifndef OTDR_EVENT_CORE_V8_H
#define OTDR_EVENT_CORE_V8_H
/*
 * OTDR Event Core v8 (C99)
 * ========================
 * v8 adds to v2:
 *
 *   1) MULTI-SCALE feature extraction:
 *      - Run envelope/SNR at multiple window sizes (e.g. 128, 512, 2048, 8192)
 *      - Per-sample: max-SNR across scales, dominant scale index
 *      → catches both narrow spikes (small window) and broad steps (large window)
 *
 *   2) POLARITY PATTERNS:
 *      - Classifies delta sign sequences in a local neighborhood
 *      - Patterns: SPIKE_POS, SPIKE_NEG, STEP_DOWN, STEP_UP,
 *                  SPIKE_STEP (connector), OSCILLATION, FLAT
 *      → richer event taxonomy than v2's binary reflect/attenuate
 *
 *   3) SMART RUN-LENGTH MERGE:
 *      - Configurable merge rules table (which pairs can merge, max gap, min runs)
 *      - Iterative merging with priority (strongest events absorb neighbors)
 *      → replaces v2's hardcoded REFLECT+ATTEN merge
 *
 *   4) PER-SEGMENT PASS/FAIL:
 *      - Configurable thresholds per event type (max loss, min SNR, max length)
 *      - Each segment gets PASS/FAIL/WARN verdict + fail reason bitmask
 *      → enables automated link-map quality assessment
 *
 * Pipeline:
 *   amp[] → multi-scale features → polarity → classify → RL-merge → pass/fail
 *
 * Build:
 *   cc -O2 -std=c99 -Wall -Wextra -pedantic otdr_event_core_v8.c -lm -c
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════ */

#define OTDR_V8_MAX_SCALES   8
#define OTDR_V8_MAX_MERGE_RULES 16

/* ═══════════════════════════════════════════════════════════════════
 * Event types (extended from v2)
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    OTDR8_EVT_NONE         = 0,
    OTDR8_EVT_REFLECTION   = 1,   /* positive spike (Fresnel) */
    OTDR8_EVT_ATTENUATION  = 2,   /* step-down (splice loss) */
    OTDR8_EVT_REFLECT_LOSS = 3,   /* merged: spike + step (connector) */
    OTDR8_EVT_GAINUP       = 4,   /* step-up (gain/splice in reverse dir) */
    OTDR8_EVT_SPIKE_NEG    = 5,   /* negative spike (dropout/glitch) */
    OTDR8_EVT_OSCILLATION  = 6,   /* rapid sign alternation (ringing) */
    OTDR8_EVT_BROADLOSS    = 7    /* gradual extended loss (bend/macrobend) */
} otdr8_event_t;

/* Polarity pattern detected in local neighborhood */
typedef enum {
    OTDR8_PAT_FLAT      = 0,
    OTDR8_PAT_SPIKE_POS = 1,   /* +peak then returns to baseline */
    OTDR8_PAT_SPIKE_NEG = 2,   /* -dip then returns */
    OTDR8_PAT_STEP_DOWN = 3,   /* sustained negative shift */
    OTDR8_PAT_STEP_UP   = 4,   /* sustained positive shift */
    OTDR8_PAT_SPIKE_STEP= 5,   /* spike followed by step (connector) */
    OTDR8_PAT_OSCILLATE = 6    /* alternating sign sequence */
} otdr8_polarity_t;

/* Pass/fail verdict */
typedef enum {
    OTDR8_VERDICT_PASS = 0,
    OTDR8_VERDICT_WARN = 1,
    OTDR8_VERDICT_FAIL = 2
} otdr8_verdict_t;

/* Fail reason bitmask */
enum {
    OTDR8_FAIL_NONE       = 0u,
    OTDR8_FAIL_HIGH_LOSS  = 1u << 0,   /* loss exceeds threshold */
    OTDR8_FAIL_LOW_SNR    = 1u << 1,   /* SNR below minimum */
    OTDR8_FAIL_TOO_LONG   = 1u << 2,   /* event region too wide */
    OTDR8_FAIL_HIGH_REFL  = 1u << 3,   /* reflectance too high */
    OTDR8_FAIL_PATTERN    = 1u << 4    /* suspicious polarity pattern */
};

/* ═══════════════════════════════════════════════════════════════════
 * Multi-scale features (per sample)
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    float   amp;
    float   delta;

    /* Per-scale envelope RMS and SNR */
    float   env_rms[OTDR_V8_MAX_SCALES];
    float   snr_db[OTDR_V8_MAX_SCALES];

    /* Aggregate across scales */
    float   max_snr_db;         /* max SNR across all scales */
    uint8_t best_scale;         /* index of scale with max SNR */

    /* Noise */
    float   noise_sigma;        /* local MAD sigma (from finest scale) */

    /* Polarity */
    otdr8_polarity_t polarity;
} otdr8_features_t;

/* Per-sample classification result */
typedef struct {
    otdr8_event_t label;
    float         confidence;   /* 0..1 */
} otdr8_result_t;

/* ═══════════════════════════════════════════════════════════════════
 * Segment with pass/fail
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    size_t         start;       /* inclusive */
    size_t         end;         /* inclusive */
    otdr8_event_t  label;
    float          mean_conf;
    float          peak_snr_db;
    float          peak_amplitude;
    otdr8_polarity_t dominant_polarity;
    uint32_t       flags;       /* merge flags etc */

    /* pass/fail */
    otdr8_verdict_t verdict;
    uint32_t        fail_reasons;  /* bitmask of OTDR8_FAIL_* */
} otdr8_segment_t;

/* ═══════════════════════════════════════════════════════════════════
 * Configuration
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Multi-scale windows */
    size_t  scale_windows[OTDR_V8_MAX_SCALES];
    size_t  num_scales;         /* 1..OTDR_V8_MAX_SCALES */

    /* Noise estimation */
    float   mad_scale;
    int     local_sigma_enable;
    size_t  sigma_window;
    size_t  sigma_stride;
    float   sigma_min;

    /* Detection thresholds */
    float   thr_reflect_snr_db;
    float   thr_atten_snr_db;
    float   thr_spike_neg_snr_db;
    float   thr_gainup_snr_db;
    float   thr_oscillation_snr_db;
    float   thr_broadloss_snr_db;
    float   min_env_rms;

    /* Polarity detection */
    size_t  polarity_halfwin;   /* half-window for pattern analysis */

    /* SNR clamping */
    float   snr_floor_db;
    float   snr_ceil_db;
} otdr8_config_t;

/* Merge rule: event type A + gap + event type B → merged type C */
typedef struct {
    otdr8_event_t from_a;
    otdr8_event_t from_b;
    otdr8_event_t merged_to;
    size_t        max_gap;      /* max NONE samples between A and B */
    size_t        min_len_a;    /* minimum run length for A */
    size_t        min_len_b;    /* minimum run length for B */
    float         min_conf;     /* minimum mean confidence */
} otdr8_merge_rule_t;

typedef struct {
    otdr8_merge_rule_t rules[OTDR_V8_MAX_MERGE_RULES];
    size_t             num_rules;
    int                iterative;    /* repeat merging until stable */
} otdr8_merge_config_t;

/* Pass/fail thresholds */
typedef struct {
    float   max_loss_db;        /* max acceptable attenuation */
    float   max_reflectance_db; /* max acceptable reflection */
    float   min_snr_db;         /* minimum acceptable SNR */
    size_t  max_event_length;   /* max samples for a single event */
    float   warn_factor;        /* warn at this fraction of fail threshold */
} otdr8_passfail_config_t;

/* ═══════════════════════════════════════════════════════════════════
 * API
 * ═══════════════════════════════════════════════════════════════════ */

otdr8_config_t         otdr8_default_config(void);
otdr8_merge_config_t   otdr8_default_merge_config(void);
otdr8_passfail_config_t otdr8_default_passfail_config(void);

/** Multi-scale feature extraction + polarity patterns */
int otdr8_extract_features(const float *amp, size_t n,
                           const otdr8_config_t *cfg,
                           otdr8_features_t *out_feat);

/** Classification using multi-scale SNR + polarity */
int otdr8_classify(const otdr8_features_t *feat, size_t n,
                   const otdr8_config_t *cfg,
                   otdr8_result_t *out_res);

/** Full pipeline: features + classify */
int otdr8_detect(const float *amp, size_t n,
                 const otdr8_config_t *cfg,
                 otdr8_features_t *out_feat,    /* optional */
                 otdr8_result_t *out_res);

/** RLE segmentation + smart merge. Returns number of segments. */
size_t otdr8_segment_merge(const otdr8_result_t *res,
                           const otdr8_features_t *feat,  /* for metrics */
                           size_t n,
                           const otdr8_merge_config_t *mcfg,
                           otdr8_segment_t *seg_out,
                           size_t max_seg);

/** Apply pass/fail to segments in-place. */
void otdr8_apply_passfail(otdr8_segment_t *segs, size_t nseg,
                          const otdr8_passfail_config_t *pf);

/** String helpers */
const char *otdr8_event_str(otdr8_event_t e);
const char *otdr8_polarity_str(otdr8_polarity_t p);
const char *otdr8_verdict_str(otdr8_verdict_t v);

#ifdef __cplusplus
}
#endif
#endif /* OTDR_EVENT_CORE_V8_H */
