#ifndef OTDR_EVENT_CORE_V10_H
#define OTDR_EVENT_CORE_V10_H
/*
 * OTDR Event Core v10 (C99)
 * =========================
 * Multi-Pass Consensus Decoding — Confidence Map
 *
 * Fuses three orthogonal quality signals into a single per-sample
 * confidence value and per-segment ranking:
 *
 *   1) AGREEMENT (from v7): How many revolutions agree at each sample?
 *      → High agreement = high confidence that sample is correct
 *
 *   2) SNR (from v8): Signal-to-noise at each sample (multi-scale max)
 *      → High SNR = strong signal, low uncertainty
 *
 *   3) INTEGRITY (from v9): Per-sample repair flags
 *      → Clean sample = full trust; flagged = reduced/zero confidence
 *
 * Confidence formula (per sample):
 *   conf[i] = w_agree * agree[i] + w_snr * snr_norm[i] + w_integ * integ[i]
 *
 *   where:
 *     agree[i]    = agreement ratio (0..1) from v7 label stability
 *     snr_norm[i] = clamp((snr_db[i] - snr_floor) / (snr_ceil - snr_floor), 0, 1)
 *     integ[i]    = 1.0 if clean, 0.0 if flagged (from v9 flags)
 *     w_agree + w_snr + w_integ = 1.0
 *
 * Segment ranking:
 *   Each segment gets a confidence score = mean(conf[start..end])
 *   Segments are ranked best→worst for decode priority
 *
 * Build:
 *   cc -O2 -std=c99 -Wall -Wextra -pedantic otdr_event_core_v10.c -lm -c
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════
 * Configuration
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Fusion weights (must sum to 1.0) */
    float w_agreement;     /* weight for multi-pass agreement (default 0.4) */
    float w_snr;           /* weight for SNR component (default 0.35) */
    float w_integrity;     /* weight for integrity flags (default 0.25) */

    /* SNR normalization */
    float snr_floor_db;    /* SNR below this → 0.0 (default -10) */
    float snr_ceil_db;     /* SNR above this → 1.0 (default 40) */

    /* Integrity mapping */
    float integ_clean;     /* confidence for clean sample (default 1.0) */
    float integ_flagged;   /* confidence for flagged sample (default 0.0) */
    float integ_repaired;  /* confidence for repaired sample (default 0.3) */

    /* Segment parameters */
    size_t min_segment_len;  /* minimum segment for ranking (default 16) */

    /* Missing-input defaults (when a source is not provided) */
    float default_agreement; /* used when agree==NULL (default 0.5) */
    float default_snr_db;    /* used when snr==NULL (default 10.0) */
} otdr10_config_t;

/* ═══════════════════════════════════════════════════════════════════
 * Per-sample confidence output
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    float confidence;     /* combined 0..1 */
    float agree_comp;     /* agreement component (weighted) */
    float snr_comp;       /* SNR component (weighted) */
    float integ_comp;     /* integrity component (weighted) */
} otdr10_sample_t;

/* ═══════════════════════════════════════════════════════════════════
 * Segment with confidence ranking
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    size_t start;
    size_t end;
    float  mean_confidence;
    float  min_confidence;
    float  mean_agreement;
    float  mean_snr_norm;
    float  mean_integrity;
    size_t flagged_count;     /* samples with integrity flags */
    size_t rank;              /* 0 = best segment */
} otdr10_segment_t;

/* ═══════════════════════════════════════════════════════════════════
 * Summary
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    size_t n;
    float  mean_confidence;
    float  min_confidence;
    float  max_confidence;
    float  median_confidence;

    float  mean_agreement;
    float  mean_snr_norm;
    float  mean_integrity;

    size_t high_conf_count;   /* conf >= 0.8 */
    size_t mid_conf_count;    /* 0.4 <= conf < 0.8 */
    size_t low_conf_count;    /* conf < 0.4 */

    float  high_conf_frac;
    float  low_conf_frac;

    size_t num_segments;
    float  overall_quality;   /* 0..1 composite */
} otdr10_summary_t;

/* ═══════════════════════════════════════════════════════════════════
 * API
 * ═══════════════════════════════════════════════════════════════════ */

otdr10_config_t otdr10_default_config(void);

/**
 * Compute per-sample confidence map.
 *
 * @param agree     Per-sample agreement ratio (0..1) from v7. NULL = use default.
 * @param snr_db    Per-sample SNR in dB from v8. NULL = use default.
 * @param flags     Per-sample integrity flags from v9. NULL = assume clean.
 * @param n         Number of samples
 * @param cfg       Configuration
 * @param out       Per-sample confidence output (caller-allocated, length n)
 * @return 0 on success, <0 on error
 */
int otdr10_compute(const float *agree,
                   const float *snr_db,
                   const uint8_t *flags,
                   size_t n,
                   const otdr10_config_t *cfg,
                   otdr10_sample_t *out);

/**
 * Segment the confidence map and rank segments.
 * Segments are contiguous runs where confidence stays above/below thresholds.
 *
 * @param samples   Per-sample confidence from otdr10_compute
 * @param n         Number of samples
 * @param cfg       Configuration
 * @param seg_out   Output segments (caller-allocated)
 * @param max_seg   Capacity
 * @return Number of segments
 */
size_t otdr10_segment_rank(const otdr10_sample_t *samples, size_t n,
                           const otdr10_config_t *cfg,
                           otdr10_segment_t *seg_out, size_t max_seg);

/**
 * Compute summary statistics.
 */
int otdr10_summarize(const otdr10_sample_t *samples, size_t n,
                     const otdr10_segment_t *segs, size_t nseg,
                     otdr10_summary_t *out);

#ifdef __cplusplus
}
#endif
#endif /* OTDR_EVENT_CORE_V10_H */
