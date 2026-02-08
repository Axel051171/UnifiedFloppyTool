/**
 * @file uft_denoise_bridge.h
 * @brief UFT ↔ φ-OTDR Denoise Integration Bridge
 *
 * Connects the φ-OTDR adaptive wavelet denoiser (SWT + MAD + soft/hard
 * thresholding, inspired by Li et al. 2023 Opt.Commun. 545:129708) to
 * UFT's flux analysis pipeline.
 *
 * Integration points:
 *   1. Raw flux interval denoising (uint32_t ns → denoised float ns)
 *   2. Analog sample denoising (int16_t → denoised float)
 *   3. Pre-OTDR smoothing (denoise before signal analysis)
 *   4. Per-revolution / per-track batch denoising
 *
 * Usage:
 *   uft_denoise_ctx_t ctx;
 *   uft_denoise_init(&ctx, NULL);          // default config
 *   uft_denoise_flux_ns(&ctx, flux, n, out_flux);   // denoise flux
 *   uft_denoise_report_t rpt = uft_denoise_get_report(&ctx);
 *   uft_denoise_free(&ctx);
 */

#ifndef UFT_DENOISE_BRIDGE_H
#define UFT_DENOISE_BRIDGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════
 * Configuration
 * ═══════════════════════════════════════════════════════════════════ */

/** Thresholding mode */
typedef enum {
    UFT_DN_SOFT = 0,    /**< Soft thresholding (smoother, default) */
    UFT_DN_HARD = 1     /**< Hard thresholding (preserves edges) */
} uft_denoise_mode_t;

/** Denoise configuration */
typedef struct {
    uint8_t  levels;           /**< SWT decomposition levels (1-8, default 5) */
    uft_denoise_mode_t mode;   /**< Soft or hard thresholding */
    float    thresh_scale;     /**< Threshold multiplier (default 1.0) */
    float    level_gains[8];   /**< Per-level gain (0 = use 1.0) */
    bool     use_level_gains;  /**< Apply per-level gains */

    /* Quiet region detection */
    bool     auto_quiet;       /**< Auto-detect quiet regions (default true) */
    size_t   quiet_window;     /**< Window size for variance estimation */
    float    quiet_keep_frac;  /**< Fraction of quietest windows (0.0-1.0) */

    /* Flux-specific */
    bool     remove_dc;        /**< Subtract mean before denoising (default true) */
    bool     preserve_integral;/**< Scale output to preserve total flux time */
    float    outlier_sigma;    /**< Clamp outliers at ±N sigma before SWT (0=off) */
} uft_denoise_config_t;

/* ═══════════════════════════════════════════════════════════════════
 * Results / Diagnostics
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    float    sigma_est;        /**< Estimated noise σ (from MAD) */
    float    snr_input_db;     /**< Input SNR estimate (dB) */
    float    snr_output_db;    /**< Output SNR estimate (dB) */
    float    snr_gain_db;      /**< SNR improvement (dB) */
    float    mse_reduction;    /**< MSE ratio (input/output, >1 = improvement) */

    float    thresh_per_level[8]; /**< Threshold applied at each level */
    uint8_t  num_levels;       /**< Levels actually used */

    size_t   samples_processed;/**< Total samples denoised */
    size_t   quiet_samples;    /**< Samples identified as quiet */
    float    quiet_fraction;   /**< Fraction of signal marked quiet */
} uft_denoise_report_t;

typedef struct {
    uft_denoise_config_t cfg;
    uft_denoise_report_t report;
    bool     initialized;
} uft_denoise_ctx_t;

/* ═══════════════════════════════════════════════════════════════════
 * Error codes
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_DN_OK            =  0,
    UFT_DN_ERR_NULL      = -1,
    UFT_DN_ERR_NOMEM     = -2,
    UFT_DN_ERR_TOO_SMALL = -3,
    UFT_DN_ERR_CONFIG    = -4,
    UFT_DN_ERR_INTERNAL  = -5
} uft_denoise_error_t;

/* ═══════════════════════════════════════════════════════════════════
 * API: Init / Free
 * ═══════════════════════════════════════════════════════════════════ */

/** Get default configuration */
uft_denoise_config_t uft_denoise_default_config(void);

/** Initialize context (cfg may be NULL for defaults) */
uft_denoise_error_t uft_denoise_init(uft_denoise_ctx_t *ctx,
                                      const uft_denoise_config_t *cfg);

/** Free context resources */
void uft_denoise_free(uft_denoise_ctx_t *ctx);

/* ═══════════════════════════════════════════════════════════════════
 * API: Denoise operations
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * Denoise flux intervals (nanoseconds).
 * Input: raw uint32_t flux intervals.
 * Output: denoised float flux intervals (same length).
 * The denoiser operates on timing jitter while preserving the mean period.
 */
uft_denoise_error_t uft_denoise_flux_ns(uft_denoise_ctx_t *ctx,
                                          const uint32_t *flux_ns,
                                          size_t count,
                                          float *out_flux_ns);

/**
 * Denoise float signal in-place or out-of-place.
 * Generic: works for any 1D float array (flux, analog, histogram).
 * If out is NULL, denoises in-place (modifies in).
 */
uft_denoise_error_t uft_denoise_float(uft_denoise_ctx_t *ctx,
                                        const float *in, float *out,
                                        size_t count);

/**
 * Denoise analog samples (int16_t → float).
 * Converts to float, denoises, writes to out_float.
 */
uft_denoise_error_t uft_denoise_analog(uft_denoise_ctx_t *ctx,
                                         const int16_t *samples,
                                         size_t count,
                                         float *out_float);

/**
 * Denoise with explicit quiet mask.
 * quiet_mask[i] = 1 for known-quiet regions (used for noise estimation).
 */
uft_denoise_error_t uft_denoise_float_masked(uft_denoise_ctx_t *ctx,
                                               const float *in, float *out,
                                               size_t count,
                                               const uint8_t *quiet_mask);

/* ═══════════════════════════════════════════════════════════════════
 * API: Results
 * ═══════════════════════════════════════════════════════════════════ */

/** Get report from last denoise operation */
uft_denoise_report_t uft_denoise_get_report(const uft_denoise_ctx_t *ctx);

/** Error string */
const char *uft_denoise_error_str(uft_denoise_error_t err);

/** Module version */
const char *uft_denoise_version(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DENOISE_BRIDGE_H */
