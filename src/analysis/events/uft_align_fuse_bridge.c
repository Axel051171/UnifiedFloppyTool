/**
 * uft_align_fuse_bridge.c — UFT ↔ OTDR v7 Alignment + Fusion Bridge
 * ====================================================================
 *
 * Wraps otdr_align_fuse_v7 for multi-revolution floppy flux analysis.
 * Adds auto-reference selection, quality scoring, and stability metrics.
 */

#include "otdr_event_core_v7.h"
#include "uft_align_fuse_bridge.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#define ALN_BRIDGE_VERSION "1.0.0"

/* ════════════════════════════════════════════════════════════════════
 * Helpers
 * ════════════════════════════════════════════════════════════════════ */

static void free_aligned_bufs(uft_align_ctx_t *ctx) {
    if (ctx->aligned_bufs) {
        for (size_t i = 0; i < ctx->buf_count; i++)
            free(ctx->aligned_bufs[i]);
        free(ctx->aligned_bufs);
        ctx->aligned_bufs = NULL;
    }
    ctx->buf_count = 0;
    ctx->buf_len = 0;
}

/* Select reference as the revolution with highest total energy (least dropout) */
static size_t auto_select_ref(const float *const *revs, size_t m, size_t n) {
    size_t best = 0;
    double best_energy = -1.0;
    for (size_t k = 0; k < m; k++) {
        double e = 0.0;
        for (size_t i = 0; i < n; i++) {
            double v = (double)revs[k][i];
            e += v * v;
        }
        if (e > best_energy) { best_energy = e; best = k; }
    }
    return best;
}

/* ════════════════════════════════════════════════════════════════════
 * Public: Config / Init / Free
 * ════════════════════════════════════════════════════════════════════ */

uft_align_config_t uft_align_default_config(void) {
    uft_align_config_t c;
    memset(&c, 0, sizeof(c));
    c.ref_rev           = 0;
    c.max_shift         = 64;
    c.auto_ref          = false;
    c.min_ncc_score     = 0.5f;
    c.num_event_classes = 4;
    return c;
}

uft_align_error_t uft_align_init(uft_align_ctx_t *ctx,
                                  const uft_align_config_t *cfg) {
    if (!ctx) return UFT_ALN_ERR_NULL;
    memset(ctx, 0, sizeof(*ctx));
    ctx->cfg = cfg ? *cfg : uft_align_default_config();

    if (ctx->cfg.max_shift < 1) ctx->cfg.max_shift = 1;
    if (ctx->cfg.num_event_classes < 2) ctx->cfg.num_event_classes = 2;
    if (ctx->cfg.num_event_classes > 32) ctx->cfg.num_event_classes = 32;

    ctx->initialized = true;
    return UFT_ALN_OK;
}

void uft_align_free(uft_align_ctx_t *ctx) {
    if (!ctx) return;
    free_aligned_bufs(ctx);
    free(ctx->rev_info);
    free(ctx->agree_ratio);
    free(ctx->entropy_like);
    memset(ctx, 0, sizeof(*ctx));
}

/* ════════════════════════════════════════════════════════════════════
 * Internal: core align + fuse
 * ════════════════════════════════════════════════════════════════════ */

static uft_align_error_t align_fuse_core(uft_align_ctx_t *ctx,
                                           const float *const *revs,
                                           size_t m, size_t n,
                                           float *out_fused) {
    if (!ctx || !ctx->initialized) return UFT_ALN_ERR_NULL;
    if (!revs || !out_fused) return UFT_ALN_ERR_NULL;
    if (m < 2) return UFT_ALN_ERR_SMALL;
    if (n < 32) return UFT_ALN_ERR_SMALL;

    uft_align_config_t *cfg = &ctx->cfg;
    uft_align_report_t *rpt = &ctx->report;
    memset(rpt, 0, sizeof(*rpt));
    rpt->num_revolutions = m;
    rpt->samples_per_rev = n;

    /* Select reference */
    size_t ref_idx = cfg->ref_rev;
    if (cfg->auto_ref || ref_idx >= m) {
        ref_idx = auto_select_ref(revs, m, n);
    }
    rpt->ref_revolution = ref_idx;

    /* Allocate per-rev info */
    free(ctx->rev_info);
    ctx->rev_info = (uft_rev_alignment_t *)calloc(m, sizeof(uft_rev_alignment_t));
    if (!ctx->rev_info) return UFT_ALN_ERR_NOMEM;
    ctx->rev_count = m;

    /* Allocate aligned buffers */
    free_aligned_bufs(ctx);
    ctx->aligned_bufs = (float **)calloc(m, sizeof(float *));
    if (!ctx->aligned_bufs) return UFT_ALN_ERR_NOMEM;
    ctx->buf_count = m;
    ctx->buf_len = n;

    for (size_t k = 0; k < m; k++) {
        ctx->aligned_bufs[k] = (float *)calloc(n, sizeof(float));
        if (!ctx->aligned_bufs[k]) return UFT_ALN_ERR_NOMEM;
    }

    /* Align */
    int *shifts = (int *)calloc(m, sizeof(int));
    if (!shifts) return UFT_ALN_ERR_NOMEM;

    int rc = otdr_align_traces(revs, m, n, ref_idx,
                                cfg->max_shift, shifts, ctx->aligned_bufs);
    if (rc != 0) {
        free(shifts);
        return UFT_ALN_ERR_INTERNAL;
    }

    /* Collect per-rev alignment info */
    double ncc_sum = 0.0;
    float worst_ncc = 1.0f;
    int max_abs = 0;
    size_t valid_count = 0;

    for (size_t k = 0; k < m; k++) {
        float score = 1.0f;
        if (k != ref_idx) {
            otdr_estimate_shift_ncc(revs[ref_idx], revs[k], n,
                                     cfg->max_shift, &score);
        }

        ctx->rev_info[k].shift     = shifts[k];
        ctx->rev_info[k].ncc_score = score;
        ctx->rev_info[k].valid     = (score >= cfg->min_ncc_score);

        ncc_sum += (double)score;
        if (score < worst_ncc) worst_ncc = score;
        if (abs(shifts[k]) > max_abs) max_abs = abs(shifts[k]);
        if (ctx->rev_info[k].valid) valid_count++;
    }

    free(shifts);

    rpt->mean_ncc         = (float)(ncc_sum / (double)m);
    rpt->worst_ncc        = worst_ncc;
    rpt->max_abs_shift    = max_abs;
    rpt->valid_alignments = valid_count;

    /* Fuse (median) */
    rc = otdr_fuse_aligned_median(ctx->aligned_bufs, m, n, out_fused);
    if (rc != 0) return UFT_ALN_ERR_INTERNAL;

    /* Quality score */
    rpt->alignment_quality = rpt->mean_ncc *
        ((float)valid_count / (float)m);
    if (rpt->alignment_quality > 1.0f) rpt->alignment_quality = 1.0f;

    return UFT_ALN_OK;
}

/* ════════════════════════════════════════════════════════════════════
 * Public: Alignment + Fusion
 * ════════════════════════════════════════════════════════════════════ */

uft_align_error_t uft_align_fuse_float(uft_align_ctx_t *ctx,
                                         const float *const *revs,
                                         size_t m, size_t n,
                                         float *out_fused) {
    return align_fuse_core(ctx, revs, m, n, out_fused);
}

uft_align_error_t uft_align_fuse_flux_ns(uft_align_ctx_t *ctx,
                                           const uint32_t *const *revs,
                                           size_t m, size_t n,
                                           float *out_fused) {
    if (!revs) return UFT_ALN_ERR_NULL;
    if (m < 2 || n < 32) return UFT_ALN_ERR_SMALL;

    /* Convert all revolutions to float */
    float **frevs = (float **)calloc(m, sizeof(float *));
    if (!frevs) return UFT_ALN_ERR_NOMEM;

    for (size_t k = 0; k < m; k++) {
        frevs[k] = (float *)malloc(n * sizeof(float));
        if (!frevs[k]) {
            for (size_t j = 0; j < k; j++) free(frevs[j]);
            free(frevs);
            return UFT_ALN_ERR_NOMEM;
        }
        for (size_t i = 0; i < n; i++)
            frevs[k][i] = (float)revs[k][i];
    }

    uft_align_error_t rc = align_fuse_core(ctx,
        (const float *const *)frevs, m, n, out_fused);

    for (size_t k = 0; k < m; k++) free(frevs[k]);
    free(frevs);
    return rc;
}

uft_align_error_t uft_align_label_stability(uft_align_ctx_t *ctx,
                                              const uint8_t *const *labels,
                                              size_t m, size_t n) {
    if (!ctx || !ctx->initialized) return UFT_ALN_ERR_NULL;
    if (!labels || m < 2 || n < 1) return UFT_ALN_ERR_SMALL;

    /* Allocate stability arrays */
    free(ctx->agree_ratio);
    free(ctx->entropy_like);
    ctx->agree_ratio  = (float *)calloc(n, sizeof(float));
    ctx->entropy_like = (float *)calloc(n, sizeof(float));
    if (!ctx->agree_ratio || !ctx->entropy_like) return UFT_ALN_ERR_NOMEM;
    ctx->stability_len = n;

    int rc = otdr_label_stability(labels, m, n,
                                   ctx->cfg.num_event_classes,
                                   ctx->agree_ratio, ctx->entropy_like);
    if (rc != 0) return UFT_ALN_ERR_INTERNAL;

    /* Compute stability metrics */
    uft_stability_metrics_t *st = &ctx->report.stability;
    memset(st, 0, sizeof(*st));

    double agree_sum = 0.0, entropy_sum = 0.0;
    float min_agree = 1.0f, max_entropy = 0.0f;
    size_t unstable = 0;

    for (size_t i = 0; i < n; i++) {
        agree_sum   += (double)ctx->agree_ratio[i];
        entropy_sum += (double)ctx->entropy_like[i];
        if (ctx->agree_ratio[i] < min_agree) min_agree = ctx->agree_ratio[i];
        if (ctx->entropy_like[i] > max_entropy) max_entropy = ctx->entropy_like[i];
        if (ctx->agree_ratio[i] < 0.5f) unstable++;
    }

    st->mean_agreement    = (float)(agree_sum / (double)n);
    st->min_agreement     = min_agree;
    st->mean_entropy      = (float)(entropy_sum / (double)n);
    st->max_entropy       = max_entropy;
    st->unstable_count    = unstable;
    st->unstable_fraction = (float)unstable / (float)n;

    ctx->report.has_stability = true;
    return UFT_ALN_OK;
}

/* ════════════════════════════════════════════════════════════════════
 * Public: Results / Utility
 * ════════════════════════════════════════════════════════════════════ */

const uft_rev_alignment_t *uft_align_get_rev(const uft_align_ctx_t *ctx,
                                               size_t idx) {
    if (!ctx || idx >= ctx->rev_count) return NULL;
    return &ctx->rev_info[idx];
}

const float *uft_align_get_agreement(const uft_align_ctx_t *ctx,
                                      size_t *out_len) {
    if (!ctx || !ctx->agree_ratio) return NULL;
    if (out_len) *out_len = ctx->stability_len;
    return ctx->agree_ratio;
}

const float *uft_align_get_entropy(const uft_align_ctx_t *ctx,
                                    size_t *out_len) {
    if (!ctx || !ctx->entropy_like) return NULL;
    if (out_len) *out_len = ctx->stability_len;
    return ctx->entropy_like;
}

uft_align_report_t uft_align_get_report(const uft_align_ctx_t *ctx) {
    uft_align_report_t empty;
    memset(&empty, 0, sizeof(empty));
    if (!ctx) return empty;
    return ctx->report;
}

const char *uft_align_error_str(uft_align_error_t err) {
    switch (err) {
    case UFT_ALN_OK:           return "OK";
    case UFT_ALN_ERR_NULL:     return "NULL parameter";
    case UFT_ALN_ERR_NOMEM:    return "Out of memory";
    case UFT_ALN_ERR_SMALL:    return "Data too small";
    case UFT_ALN_ERR_CONFIG:   return "Invalid configuration";
    case UFT_ALN_ERR_INTERNAL: return "Internal alignment error";
    default:                   return "Unknown error";
    }
}

const char *uft_align_version(void) {
    return ALN_BRIDGE_VERSION;
}
