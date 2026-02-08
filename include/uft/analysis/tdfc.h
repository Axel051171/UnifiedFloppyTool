#ifndef TDFC_H
#define TDFC_H

/*
 * TDFC+ — Time-Domain Flux Characterization (C99)
 * ------------------------------------------------
 * Core:
 *  - Sliding RMS envelope
 *  - Sliding SNR profile (dB) on |x|
 *  - CUSUM change-point detection (on SNR series)
 *  - Optional normalized correlation against a template
 *
 * Plus:
 *  - Robust stats: median, MAD, trimmed mean
 *  - Dropout detectors:
 *      A) Envelope-based (points): env_rms < threshold for >= min_run points
 *      B) Amplitude-based (samples): |x| < threshold for >= min_run samples,
 *         then mapped to point flags by cfg->step.
 *  - Segmentation: change-points -> segments with per-segment stats
 *
 * Build:
 *   cc -O2 -std=c99 -Wall -Wextra -pedantic tdfc.c tdfc_plus.c -lm
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tdfc_config {
    size_t env_window;     /* sliding RMS window */
    size_t snr_window;     /* sliding stats window */
    size_t step;           /* points step (samples) */

    float  cusum_k;        /* drift */
    float  cusum_h;        /* threshold */

    int    remove_dc;

    const float *template_sig;
    size_t template_len;
} tdfc_config;

typedef struct tdfc_result {
    size_t n_points;
    size_t step;

    float *envelope_rms;
    float *snr_db;
    float *corr;              /* optional, else NULL */
    uint8_t *change;          /* 0/1 flags */

    float global_mean;
    float global_std;
} tdfc_result;

/* ---- core API ---- */
tdfc_config tdfc_default_config(void);

int  tdfc_analyze(const float *signal, size_t n_samples,
                  const tdfc_config *cfg, tdfc_result *out);

void tdfc_free_result(tdfc_result *r);

void tdfc_i16_to_f32(const int16_t *in, float *out, size_t n, float scale);

/* Quick heuristic health score (0..100). */
int  tdfc_health_score(const tdfc_result *r);

/* ---- robust stats (tdfc_plus.c) ---- */

typedef struct tdfc_robust_stats_t {
    float median;
    float mad;           /* median absolute deviation */
    float sigma_mad;     /* 1.4826 * MAD (robust sigma estimate) */
    float trimmed_mean;  /* mean after trimming tails */
} tdfc_robust_stats_t;

/* Compute robust stats on array x (length n).
 * trim_frac: 0..0.49 (e.g. 0.10 trims 10% low + 10% high).
 * Returns 0 on success.
 */
int tdfc_compute_robust_stats(const float *x, size_t n, float trim_frac,
                              tdfc_robust_stats_t *out);

/* ---- dropout detection (tdfc_plus.c) ---- */

/* Variant A: Envelope-based dropout detection on points.
 * A dropout is a run of envelope_rms < threshold for >= min_run points.
 */
int tdfc_detect_dropouts_env(const float *envelope_rms, size_t n_points,
                             float threshold, size_t min_run_points,
                             uint8_t *dropout_flag_points,
                             float *dropout_ratio_points);

/* Variant B: Raw amplitude-based dropout detection on samples.
 * Detects runs where |x| < threshold for >= min_run_samples.
 * Produces:
 *   dropout_flag_samples[n_samples] (0/1) — sample-level (nullable)
 *   dropout_flag_points[n_points] (0/1)   — mapped by step (non-null)
 *   dropout_ratio_points                  — fraction (nullable)
 *
 * Note: Captures very short dropouts that get smeared by env windows.
 */
int tdfc_detect_dropouts_amp(const float *signal, size_t n_samples,
                             size_t step, float threshold,
                             size_t min_run_samples,
                             uint8_t *dropout_flag_samples,
                             uint8_t *dropout_flag_points,
                             float *dropout_ratio_points);

/* ---- segmentation (tdfc_plus.c) ---- */

enum {
    TDFC_SEG_OK                = 0u,
    TDFC_SEG_FLAG_HAS_DROPOUTS = 1u << 0,
    TDFC_SEG_FLAG_DEGRADED     = 1u << 1
};

typedef struct tdfc_segment {
    size_t start_point;   /* inclusive */
    size_t end_point;     /* inclusive */

    float mean_snr_db;
    float mean_env_rms;
    float dropout_rate;   /* 0..1 within segment */

    float score;          /* 0..100 (simple baseline) */
    uint32_t flags;
} tdfc_segment;

typedef struct tdfc_segmentation {
    tdfc_segment *seg;
    size_t n_seg;
} tdfc_segmentation;

/* Build segments from change[] flags.
 * Enforces min_seg_len_points by merging too-small segments.
 */
int  tdfc_segment_from_changepoints(const tdfc_result *r,
                                    const uint8_t *dropout_flag_points,
                                    size_t min_seg_len_points,
                                    tdfc_segmentation *out);

void tdfc_free_segmentation(tdfc_segmentation *s);

#ifdef __cplusplus
}
#endif

#endif /* TDFC_H */
