/**
 * uft_denoise_bridge.c — UFT ↔ φ-OTDR Denoise Integration Bridge
 * ================================================================
 *
 * Wraps phi_otdr_denoise_1d for use in UFT's flux analysis pipeline.
 * Adds flux-specific preprocessing (DC removal, outlier clamping,
 * integral preservation) and SNR diagnostics.
 */

#include "phi_otdr_denoise_1d.h"
#include "uft_denoise_bridge.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#define BRIDGE_VERSION "1.0.0"

/* ════════════════════════════════════════════════════════════════════
 * Helpers
 * ════════════════════════════════════════════════════════════════════ */

static double compute_variance(const float *x, size_t n) {
    if (n < 2) return 0.0;
    double sum = 0.0, sumsq = 0.0;
    for (size_t i = 0; i < n; i++) {
        double v = (double)x[i];
        sum   += v;
        sumsq += v * v;
    }
    double mean = sum / (double)n;
    return (sumsq / (double)n) - (mean * mean);
}

static double compute_mean(const float *x, size_t n) {
    if (n == 0) return 0.0;
    double sum = 0.0;
    for (size_t i = 0; i < n; i++) sum += (double)x[i];
    return sum / (double)n;
}

static double compute_rms(const float *x, size_t n) {
    double v = compute_variance(x, n);
    return (v > 0.0) ? sqrt(v) : 0.0;
}


/* ════════════════════════════════════════════════════════════════════
 * Public: Config / Init / Free
 * ════════════════════════════════════════════════════════════════════ */

uft_denoise_config_t uft_denoise_default_config(void) {
    uft_denoise_config_t c;
    memset(&c, 0, sizeof(c));
    c.levels           = 5;
    c.mode             = UFT_DN_SOFT;
    c.thresh_scale     = 1.0f;
    c.use_level_gains  = false;
    c.auto_quiet       = true;
    c.quiet_window     = 2048;
    c.quiet_keep_frac  = 0.20f;
    c.remove_dc        = true;
    c.preserve_integral = true;
    c.outlier_sigma    = 5.0f;
    return c;
}

uft_denoise_error_t uft_denoise_init(uft_denoise_ctx_t *ctx,
                                      const uft_denoise_config_t *cfg) {
    if (!ctx) return UFT_DN_ERR_NULL;
    memset(ctx, 0, sizeof(*ctx));

    if (cfg) {
        ctx->cfg = *cfg;
    } else {
        ctx->cfg = uft_denoise_default_config();
    }

    /* Validate */
    if (ctx->cfg.levels < 1) ctx->cfg.levels = 1;
    if (ctx->cfg.levels > 8) ctx->cfg.levels = 8;
    if (ctx->cfg.thresh_scale <= 0.0f) ctx->cfg.thresh_scale = 1.0f;
    if (ctx->cfg.quiet_keep_frac < 0.01f) ctx->cfg.quiet_keep_frac = 0.01f;
    if (ctx->cfg.quiet_keep_frac > 0.99f) ctx->cfg.quiet_keep_frac = 0.99f;

    ctx->initialized = true;
    return UFT_DN_OK;
}

void uft_denoise_free(uft_denoise_ctx_t *ctx) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
}

/* ════════════════════════════════════════════════════════════════════
 * Internal: core denoise with full preprocessing
 * ════════════════════════════════════════════════════════════════════ */

static uft_denoise_error_t denoise_core(uft_denoise_ctx_t *ctx,
                                          const float *in, float *out,
                                          size_t n,
                                          const uint8_t *quiet_mask) {
    if (!ctx || !ctx->initialized) return UFT_DN_ERR_NULL;
    if (!in || !out) return UFT_DN_ERR_NULL;
    if (n < 2) return UFT_DN_ERR_TOO_SMALL;

    uft_denoise_config_t *cfg = &ctx->cfg;
    uft_denoise_report_t *rpt = &ctx->report;
    memset(rpt, 0, sizeof(*rpt));
    rpt->num_levels = cfg->levels;
    rpt->samples_processed = n;

    /* Working buffer */
    float *work = (float *)malloc(n * sizeof(float));
    if (!work) return UFT_DN_ERR_NOMEM;
    memcpy(work, in, n * sizeof(float));

    /* 1) DC removal */
    double dc = 0.0;
    if (cfg->remove_dc) {
        dc = compute_mean(work, n);
        for (size_t i = 0; i < n; i++) work[i] -= (float)dc;
    }

    /* 2) Outlier clamping */
    if (cfg->outlier_sigma > 0.0f) {
        double rms = compute_rms(work, n);
        float limit = (float)(cfg->outlier_sigma * rms);
        if (limit > 0.0f) {
            for (size_t i = 0; i < n; i++) {
                if (work[i] > limit) work[i] = limit;
                else if (work[i] < -limit) work[i] = -limit;
            }
        }
    }

    /* 3) Measure input variance before denoising */
    double var_input = compute_variance(work, n);

    /* 4) Build podr_config */
    podr_config pcfg = podr_default_config();
    pcfg.levels          = cfg->levels;
    pcfg.mode            = (cfg->mode == UFT_DN_HARD) ? PODR_THRESH_HARD
                                                       : PODR_THRESH_SOFT;
    pcfg.thresh_scale    = cfg->thresh_scale;
    pcfg.quiet_window    = cfg->quiet_window;
    pcfg.quiet_keep_frac = cfg->quiet_keep_frac;

    if (cfg->use_level_gains) {
        pcfg.level_gain = cfg->level_gains;
    }

    if (quiet_mask) {
        pcfg.quiet_mask = quiet_mask;
        pcfg.auto_quiet = 0;

        /* Count quiet samples for report */
        for (size_t i = 0; i < n; i++) {
            if (quiet_mask[i]) rpt->quiet_samples++;
        }
    } else {
        pcfg.auto_quiet = cfg->auto_quiet ? 1 : 0;
    }

    /* 5) Run denoiser */
    podr_diag diag;
    memset(&diag, 0, sizeof(diag));

    int rc = podr_denoise_1d(work, out, n, &pcfg, &diag);
    if (rc != 0) {
        free(work);
        podr_free_diag(&diag);
        return UFT_DN_ERR_INTERNAL;
    }

    /* 6) Collect diagnostics */
    rpt->sigma_est = diag.sigma_est;
    if (diag.thr_per_level) {
        for (uint8_t l = 0; l < cfg->levels && l < 8; l++) {
            rpt->thresh_per_level[l] = diag.thr_per_level[l];
        }
    }
    podr_free_diag(&diag);

    /* If auto_quiet was used, estimate quiet fraction from variance */
    if (!quiet_mask && cfg->auto_quiet) {
        uint8_t *qm = (uint8_t *)malloc(n);
        if (qm) {
            if (podr_build_auto_quiet_mask(in, n, cfg->quiet_window,
                                            cfg->quiet_keep_frac, qm) == 0) {
                for (size_t i = 0; i < n; i++) {
                    if (qm[i]) rpt->quiet_samples++;
                }
            }
            free(qm);
        }
    }
    rpt->quiet_fraction = (n > 0)
        ? (float)rpt->quiet_samples / (float)n : 0.0f;

    /* 7) Restore DC */
    if (cfg->remove_dc) {
        for (size_t i = 0; i < n; i++) out[i] += (float)dc;
    }

    /* 8) Preserve integral (total flux time) */
    if (cfg->preserve_integral) {
        double sum_in = 0.0, sum_out = 0.0;
        for (size_t i = 0; i < n; i++) {
            sum_in  += (double)in[i];
            sum_out += (double)out[i];
        }
        if (fabs(sum_out) > 1e-30 && fabs(sum_in) > 1e-30) {
            float scale = (float)(sum_in / sum_out);
            for (size_t i = 0; i < n; i++) out[i] *= scale;
        }
    }

    /* 9) Compute SNR metrics */
    double var_output = compute_variance(out, n);

    /* SNR estimates: assume noise variance ≈ sigma² */
    double noise_var = (double)rpt->sigma_est * (double)rpt->sigma_est;
    if (noise_var > 1e-30) {
        double sig_var_in = var_input - noise_var;
        if (sig_var_in < 0.0) sig_var_in = var_input * 0.5;
        rpt->snr_input_db = (float)(10.0 * log10(sig_var_in / noise_var));

        double noise_var_out = var_output - (var_output > noise_var
                                ? var_output - noise_var : 0.0);
        if (noise_var_out < 1e-30) noise_var_out = noise_var * 0.01;
        /* Approximate output SNR from variance reduction */
        double sig_var_out = var_output;
        rpt->snr_output_db = (float)(10.0 * log10(sig_var_out / noise_var_out));
    }

    if (var_output > 1e-30) {
        rpt->mse_reduction = (float)(var_input / var_output);
    }
    rpt->snr_gain_db = rpt->snr_output_db - rpt->snr_input_db;

    free(work);
    return UFT_DN_OK;
}

/* ════════════════════════════════════════════════════════════════════
 * Public: Denoise operations
 * ════════════════════════════════════════════════════════════════════ */

uft_denoise_error_t uft_denoise_flux_ns(uft_denoise_ctx_t *ctx,
                                          const uint32_t *flux_ns,
                                          size_t count,
                                          float *out_flux_ns) {
    if (!flux_ns || !out_flux_ns) return UFT_DN_ERR_NULL;
    if (count < 2) return UFT_DN_ERR_TOO_SMALL;

    /* Convert uint32 → float */
    float *fin = (float *)malloc(count * sizeof(float));
    if (!fin) return UFT_DN_ERR_NOMEM;
    for (size_t i = 0; i < count; i++) fin[i] = (float)flux_ns[i];

    uft_denoise_error_t rc = denoise_core(ctx, fin, out_flux_ns, count, NULL);

    /* Clamp to non-negative (flux intervals can't be negative) */
    if (rc == UFT_DN_OK) {
        for (size_t i = 0; i < count; i++) {
            if (out_flux_ns[i] < 0.0f) out_flux_ns[i] = 0.0f;
        }
    }

    free(fin);
    return rc;
}

uft_denoise_error_t uft_denoise_float(uft_denoise_ctx_t *ctx,
                                        const float *in, float *out,
                                        size_t count) {
    if (!in) return UFT_DN_ERR_NULL;

    /* In-place support */
    if (!out) {
        float *tmp = (float *)malloc(count * sizeof(float));
        if (!tmp) return UFT_DN_ERR_NOMEM;
        uft_denoise_error_t rc = denoise_core(ctx, in, tmp, count, NULL);
        if (rc == UFT_DN_OK) memcpy((float *)in, tmp, count * sizeof(float));
        free(tmp);
        return rc;
    }

    return denoise_core(ctx, in, out, count, NULL);
}

uft_denoise_error_t uft_denoise_analog(uft_denoise_ctx_t *ctx,
                                         const int16_t *samples,
                                         size_t count,
                                         float *out_float) {
    if (!samples || !out_float) return UFT_DN_ERR_NULL;
    if (count < 2) return UFT_DN_ERR_TOO_SMALL;

    /* Convert int16 → float (normalized to [-1, 1]) */
    float *fin = (float *)malloc(count * sizeof(float));
    if (!fin) return UFT_DN_ERR_NOMEM;
    for (size_t i = 0; i < count; i++)
        fin[i] = (float)samples[i] / 32768.0f;

    uft_denoise_error_t rc = denoise_core(ctx, fin, out_float, count, NULL);
    free(fin);
    return rc;
}

uft_denoise_error_t uft_denoise_float_masked(uft_denoise_ctx_t *ctx,
                                               const float *in, float *out,
                                               size_t count,
                                               const uint8_t *quiet_mask) {
    return denoise_core(ctx, in, out, count, quiet_mask);
}

/* ════════════════════════════════════════════════════════════════════
 * Public: Results / Utility
 * ════════════════════════════════════════════════════════════════════ */

uft_denoise_report_t uft_denoise_get_report(const uft_denoise_ctx_t *ctx) {
    uft_denoise_report_t empty;
    memset(&empty, 0, sizeof(empty));
    if (!ctx) return empty;
    return ctx->report;
}

const char *uft_denoise_error_str(uft_denoise_error_t err) {
    switch (err) {
    case UFT_DN_OK:            return "OK";
    case UFT_DN_ERR_NULL:      return "NULL parameter";
    case UFT_DN_ERR_NOMEM:     return "Out of memory";
    case UFT_DN_ERR_TOO_SMALL: return "Data too small";
    case UFT_DN_ERR_CONFIG:    return "Invalid configuration";
    case UFT_DN_ERR_INTERNAL:  return "Internal denoiser error";
    default:                   return "Unknown error";
    }
}

const char *uft_denoise_version(void) {
    return BRIDGE_VERSION;
}
