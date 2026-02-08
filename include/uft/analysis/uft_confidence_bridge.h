/**
 * @file uft_confidence_bridge.h
 * @brief UFT ↔ OTDR v10 Confidence Map Bridge
 *
 * Fuses three quality dimensions into per-sample decode confidence:
 *
 *   Source              UFT meaning                    Weight
 *   ─────────────────   ──────────────────────────     ──────
 *   v7 Agreement        Multi-rev consensus            0.40
 *   v8 SNR              Signal strength (multi-scale)  0.35
 *   v9 Integrity        Hardware/media quality          0.25
 *
 * Use cases:
 *   - Prioritize which track regions to decode first
 *   - Flag unreliable bits for error correction
 *   - Generate per-track quality heat maps
 *   - Rank multiple revolutions' segments for best-effort decode
 *
 * Pipeline position (final fusion stage):
 *   [v7 align+fuse] → agreement[]
 *   [v8 detect]     → snr_db[]       → [v10 confidence] → conf[] + segments
 *   [v9 integrity]  → flags[]
 */

#ifndef UFT_CONFIDENCE_BRIDGE_H
#define UFT_CONFIDENCE_BRIDGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════
 * Confidence band
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_CONF_HIGH = 0,   /**< ≥ 0.8: reliable decode */
    UFT_CONF_MID  = 1,   /**< 0.4–0.8: usable with caution */
    UFT_CONF_LOW  = 2    /**< < 0.4: unreliable / skip */
} uft_conf_band_t;

/* ═══════════════════════════════════════════════════════════════════
 * Per-sample output
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    float           confidence;    /**< combined 0..1 */
    float           agree_comp;    /**< agreement contribution */
    float           snr_comp;      /**< SNR contribution */
    float           integ_comp;    /**< integrity contribution */
    uft_conf_band_t band;          /**< HIGH / MID / LOW */
} uft_conf_sample_t;

/* ═══════════════════════════════════════════════════════════════════
 * Ranked segment
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    size_t          start;
    size_t          end;
    size_t          length;
    float           mean_confidence;
    float           min_confidence;
    uft_conf_band_t band;
    size_t          rank;           /**< 0 = best */
    size_t          flagged_count;
} uft_conf_segment_t;

/* ═══════════════════════════════════════════════════════════════════
 * Configuration
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Fusion weights */
    float w_agreement;
    float w_snr;
    float w_integrity;

    /* SNR normalization range */
    float snr_floor_db;
    float snr_ceil_db;

    /* Integrity values */
    float integ_clean;
    float integ_flagged;
    float integ_repaired;

    /* Segmentation */
    size_t min_segment_len;

    /* Defaults for missing inputs */
    float default_agreement;
    float default_snr_db;
} uft_conf_config_t;

/* ═══════════════════════════════════════════════════════════════════
 * Report
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    size_t samples_analyzed;

    float  mean_confidence;
    float  median_confidence;
    float  min_confidence;
    float  max_confidence;

    size_t high_count;
    size_t mid_count;
    size_t low_count;
    float  high_fraction;
    float  low_fraction;

    size_t num_segments;
    float  overall_quality;   /**< 0..1 composite score */
} uft_conf_report_t;

/* ═══════════════════════════════════════════════════════════════════
 * Context
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    uft_conf_config_t   cfg;
    uft_conf_report_t   report;

    uft_conf_sample_t  *samples;
    size_t              sample_count;

    uft_conf_segment_t *segments;
    size_t              segment_count;
    size_t              segment_capacity;

    bool initialized;
} uft_conf_ctx_t;

/* ═══════════════════════════════════════════════════════════════════
 * Error codes
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_CONF_OK           =  0,
    UFT_CONF_ERR_NULL     = -1,
    UFT_CONF_ERR_NOMEM    = -2,
    UFT_CONF_ERR_SMALL    = -3,
    UFT_CONF_ERR_INTERNAL = -4
} uft_conf_error_t;

/* ═══════════════════════════════════════════════════════════════════
 * API
 * ═══════════════════════════════════════════════════════════════════ */

uft_conf_config_t uft_conf_default_config(void);

uft_conf_error_t uft_conf_init(uft_conf_ctx_t *ctx, const uft_conf_config_t *cfg);
void             uft_conf_free(uft_conf_ctx_t *ctx);

/**
 * Compute confidence map from three input sources.
 * Any input can be NULL (defaults used).
 *
 * @param agreement  Per-sample agreement ratio (0..1) from v7. NULL OK.
 * @param snr_db     Per-sample SNR in dB from v8. NULL OK.
 * @param flags      Per-sample integrity flags from v9. NULL OK.
 * @param n          Number of samples (must match all non-NULL arrays)
 */
uft_conf_error_t uft_conf_compute(uft_conf_ctx_t *ctx,
                                    const float *agreement,
                                    const float *snr_db,
                                    const uint8_t *flags,
                                    size_t n);

/* Results */
size_t                     uft_conf_sample_count(const uft_conf_ctx_t *ctx);
const uft_conf_sample_t *  uft_conf_get_sample(const uft_conf_ctx_t *ctx, size_t idx);
size_t                     uft_conf_segment_count(const uft_conf_ctx_t *ctx);
const uft_conf_segment_t * uft_conf_get_segment(const uft_conf_ctx_t *ctx, size_t idx);
uft_conf_report_t          uft_conf_get_report(const uft_conf_ctx_t *ctx);

/* Count by band */
size_t uft_conf_count_band(const uft_conf_ctx_t *ctx, uft_conf_band_t band);

/* Utilities */
const char *uft_conf_band_str(uft_conf_band_t b);
const char *uft_conf_error_str(uft_conf_error_t e);
const char *uft_conf_version(void);

#ifdef __cplusplus
}
#endif
#endif /* UFT_CONFIDENCE_BRIDGE_H */
