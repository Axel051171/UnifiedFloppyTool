/**
 * @file uft_align_fuse_bridge.h
 * @brief UFT ↔ OTDR v7 Alignment + Fusion Bridge
 *
 * Multi-revolution alignment and fusion for floppy flux analysis.
 * Maps OTDR multi-pass concepts to floppy domain:
 *
 *   OTDR fiber domain            →  UFT floppy domain
 *   ──────────────────────────      ──────────────────────────
 *   Multiple OTDR passes         →  Multiple disk revolutions
 *   Shift estimation (NCC)       →  Revolution alignment (index hole drift)
 *   Median fusion                →  Multi-rev consensus (noise reduction)
 *   Label stability              →  Per-sample event agreement across revs
 *
 * Pipeline position:
 *   rev0,rev1,..revN → [align] → [fuse] → single clean trace → [denoise] → [event detect]
 *
 * Use cases:
 *   1. Align multiple revolutions of same track (compensate index sensor jitter)
 *   2. Fuse aligned revolutions for improved SNR (median rejects impulse noise)
 *   3. Assess per-sample stability across revolutions (identify flaky bits)
 *   4. Chain with denoise + event detection for full analysis pipeline
 */

#ifndef UFT_ALIGN_FUSE_BRIDGE_H
#define UFT_ALIGN_FUSE_BRIDGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════
 * Configuration
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    size_t  ref_rev;           /**< Reference revolution index (default 0) */
    int     max_shift;         /**< Maximum shift in samples (default 64) */
    bool    auto_ref;          /**< Auto-select best reference rev (default false) */
    float   min_ncc_score;     /**< Minimum NCC for valid alignment (default 0.5) */
    uint8_t num_event_classes; /**< Event class count for stability (default 4) */
} uft_align_config_t;

/* ═══════════════════════════════════════════════════════════════════
 * Results
 * ═══════════════════════════════════════════════════════════════════ */

/** Per-revolution alignment info */
typedef struct {
    int     shift;             /**< Estimated shift (samples) */
    float   ncc_score;         /**< NCC correlation score (0–1) */
    bool    valid;             /**< Alignment considered valid */
} uft_rev_alignment_t;

/** Stability metrics */
typedef struct {
    float   mean_agreement;    /**< Mean agreement ratio across all samples */
    float   min_agreement;     /**< Worst agreement at any sample */
    float   mean_entropy;      /**< Mean disagreement metric */
    float   max_entropy;       /**< Worst disagreement */
    size_t  unstable_count;    /**< Samples with agreement < 0.5 */
    float   unstable_fraction; /**< Fraction of unstable samples */
} uft_stability_metrics_t;

/** Full report */
typedef struct {
    size_t  num_revolutions;   /**< Number of revolutions processed */
    size_t  samples_per_rev;   /**< Samples per revolution */
    size_t  ref_revolution;    /**< Which revolution was used as reference */

    /* Alignment */
    float   mean_ncc;          /**< Mean NCC score across revolutions */
    float   worst_ncc;         /**< Worst NCC score */
    int     max_abs_shift;     /**< Largest absolute shift found */
    size_t  valid_alignments;  /**< How many revolutions aligned well */

    /* Stability (only if stability was computed) */
    bool    has_stability;
    uft_stability_metrics_t stability;

    /* Quality */
    float   alignment_quality; /**< Overall alignment quality 0–1 */
} uft_align_report_t;

/* ═══════════════════════════════════════════════════════════════════
 * Context
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    uft_align_config_t  cfg;
    uft_align_report_t  report;

    /* Per-revolution alignment results */
    uft_rev_alignment_t *rev_info;
    size_t rev_count;

    /* Internal buffers */
    float **aligned_bufs;     /**< Aligned revolution buffers */
    size_t  buf_count;
    size_t  buf_len;

    /* Stability arrays */
    float  *agree_ratio;
    float  *entropy_like;
    size_t  stability_len;

    bool initialized;
} uft_align_ctx_t;

/* ═══════════════════════════════════════════════════════════════════
 * Error codes
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_ALN_OK           =  0,
    UFT_ALN_ERR_NULL     = -1,
    UFT_ALN_ERR_NOMEM    = -2,
    UFT_ALN_ERR_SMALL    = -3,
    UFT_ALN_ERR_CONFIG   = -4,
    UFT_ALN_ERR_INTERNAL = -5
} uft_align_error_t;

/* ═══════════════════════════════════════════════════════════════════
 * API: Init / Free
 * ═══════════════════════════════════════════════════════════════════ */

uft_align_config_t uft_align_default_config(void);

uft_align_error_t uft_align_init(uft_align_ctx_t *ctx,
                                  const uft_align_config_t *cfg);

void uft_align_free(uft_align_ctx_t *ctx);

/* ═══════════════════════════════════════════════════════════════════
 * API: Alignment + Fusion
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * Align and fuse multiple float revolutions.
 * @param revs     Array of m revolution pointers, each length n
 * @param m        Number of revolutions (≥2)
 * @param n        Samples per revolution
 * @param out_fused  Output fused trace (length n, caller-allocated)
 */
uft_align_error_t uft_align_fuse_float(uft_align_ctx_t *ctx,
                                         const float *const *revs,
                                         size_t m, size_t n,
                                         float *out_fused);

/**
 * Align and fuse flux intervals (uint32_t ns).
 * Converts to float, aligns, fuses, writes back to float output.
 */
uft_align_error_t uft_align_fuse_flux_ns(uft_align_ctx_t *ctx,
                                           const uint32_t *const *revs,
                                           size_t m, size_t n,
                                           float *out_fused);

/**
 * Compute label stability across revolutions.
 * @param labels   Array of m label arrays, each length n, values in [0..num_classes-1]
 * @param m        Number of revolutions
 * @param n        Samples per revolution
 */
uft_align_error_t uft_align_label_stability(uft_align_ctx_t *ctx,
                                              const uint8_t *const *labels,
                                              size_t m, size_t n);

/* ═══════════════════════════════════════════════════════════════════
 * API: Results
 * ═══════════════════════════════════════════════════════════════════ */

/** Get alignment info for a specific revolution */
const uft_rev_alignment_t *uft_align_get_rev(const uft_align_ctx_t *ctx, size_t idx);

/** Get agreement ratio array (length = samples_per_rev, NULL if not computed) */
const float *uft_align_get_agreement(const uft_align_ctx_t *ctx, size_t *out_len);

/** Get entropy array (length = samples_per_rev, NULL if not computed) */
const float *uft_align_get_entropy(const uft_align_ctx_t *ctx, size_t *out_len);

/** Get summary report */
uft_align_report_t uft_align_get_report(const uft_align_ctx_t *ctx);

/** Error string */
const char *uft_align_error_str(uft_align_error_t err);

/** Module version */
const char *uft_align_version(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ALIGN_FUSE_BRIDGE_H */
