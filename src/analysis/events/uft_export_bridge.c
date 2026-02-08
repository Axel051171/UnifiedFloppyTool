/**
 * uft_export_bridge.c — UFT ↔ OTDR v12 Export/Integration Bridge
 * ================================================================
 */

#include "otdr_event_core_v12.h"
#include "uft_export_bridge.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define V12_BRIDGE_VERSION "1.0.0"

/* ════════════════════════════════════════════════════════════════════
 * Opaque context
 * ════════════════════════════════════════════════════════════════════ */

struct uft_export_ctx {
    otdr12_result_t result;
    int             has_result;
    int             initialized;
};

/* ════════════════════════════════════════════════════════════════════
 * Create / Destroy
 * ════════════════════════════════════════════════════════════════════ */

uft_export_error_t uft_export_create(uft_export_ctx_t **out) {
    if (!out) return UFT_EXP_ERR_NULL;
    *out = NULL;

    uft_export_ctx_t *ctx = (uft_export_ctx_t *)calloc(1, sizeof(*ctx));
    if (!ctx) return UFT_EXP_ERR_NOMEM;
    ctx->initialized = 1;
    *out = ctx;
    return UFT_EXP_OK;
}

void uft_export_destroy(uft_export_ctx_t *ctx) {
    if (!ctx) return;
    if (ctx->has_result)
        otdr12_free_result(&ctx->result);
    free(ctx);
}

/* ════════════════════════════════════════════════════════════════════
 * Analyze
 * ════════════════════════════════════════════════════════════════════ */

uft_export_error_t uft_export_analyze_float(uft_export_ctx_t *ctx,
                                              const float *signal, size_t n) {
    if (!ctx || !ctx->initialized || !signal) return UFT_EXP_ERR_NULL;
    if (n < 16) return UFT_EXP_ERR_SMALL;

    if (ctx->has_result) {
        otdr12_free_result(&ctx->result);
        ctx->has_result = 0;
    }

    int rc = otdr12_analyze(signal, n, &ctx->result);
    if (rc != 0) return UFT_EXP_ERR_INTERNAL;
    ctx->has_result = 1;
    return UFT_EXP_OK;
}

uft_export_error_t uft_export_analyze_flux_ns(uft_export_ctx_t *ctx,
                                                const uint32_t *flux, size_t n) {
    if (!ctx || !flux) return UFT_EXP_ERR_NULL;
    if (n < 16) return UFT_EXP_ERR_SMALL;

    float *f = (float *)malloc(n * sizeof(float));
    if (!f) return UFT_EXP_ERR_NOMEM;
    for (size_t i = 0; i < n; i++) f[i] = (float)flux[i];
    uft_export_error_t rc = uft_export_analyze_float(ctx, f, n);
    free(f);
    return rc;
}

uft_export_error_t uft_export_analyze_analog(uft_export_ctx_t *ctx,
                                               const int16_t *samples, size_t n) {
    if (!ctx || !samples) return UFT_EXP_ERR_NULL;
    if (n < 16) return UFT_EXP_ERR_SMALL;

    float *f = (float *)malloc(n * sizeof(float));
    if (!f) return UFT_EXP_ERR_NOMEM;
    for (size_t i = 0; i < n; i++) f[i] = (float)samples[i] / 32768.0f;
    uft_export_error_t rc = uft_export_analyze_float(ctx, f, n);
    free(f);
    return rc;
}

/* ════════════════════════════════════════════════════════════════════
 * Export
 * ════════════════════════════════════════════════════════════════════ */

uft_export_error_t uft_export_to_buffer(const uft_export_ctx_t *ctx,
                                          uft_export_format_t fmt,
                                          char *buf, size_t buflen,
                                          size_t *bytes_written) {
    if (!ctx || !ctx->has_result) return UFT_EXP_ERR_NULL;

    otdr12_format_t ofmt;
    switch (fmt) {
    case UFT_EXPORT_JSON:   ofmt = OTDR12_FMT_JSON;   break;
    case UFT_EXPORT_CSV:    ofmt = OTDR12_FMT_CSV;    break;
    case UFT_EXPORT_BINARY: ofmt = OTDR12_FMT_BINARY; break;
    default: return UFT_EXP_ERR_FORMAT;
    }

    int rc = otdr12_export(&ctx->result, ofmt, buf, buflen);
    if (rc < 0) return UFT_EXP_ERR_INTERNAL;
    if (bytes_written) *bytes_written = (size_t)rc;
    return UFT_EXP_OK;
}

/* ════════════════════════════════════════════════════════════════════
 * Results
 * ════════════════════════════════════════════════════════════════════ */

uft_export_report_t uft_export_get_report(const uft_export_ctx_t *ctx) {
    uft_export_report_t r;
    memset(&r, 0, sizeof(r));
    if (!ctx || !ctx->has_result) return r;

    const otdr12_result_t *res = &ctx->result;
    r.integrity_score   = res->integrity_score;
    r.flagged_samples   = res->flagged_samples;
    r.dropout_count     = res->dropout_count;
    r.saturated_count   = res->saturated_count;
    r.stuck_count       = res->stuck_count;
    r.mean_confidence   = res->mean_confidence;
    r.median_confidence = res->median_confidence;
    r.min_confidence    = res->min_confidence;
    r.max_confidence    = res->max_confidence;
    r.high_conf_count   = res->high_conf_count;
    r.mid_conf_count    = res->mid_conf_count;
    r.low_conf_count    = res->low_conf_count;
    r.n_events          = res->n_events;
    r.n_segments        = res->n_segments;
    r.overall_quality   = res->overall_quality;
    r.n_samples         = res->n_samples;
    return r;
}

bool uft_export_has_result(const uft_export_ctx_t *ctx) {
    return ctx && ctx->has_result;
}

/* ════════════════════════════════════════════════════════════════════
 * Golden vectors
 * ════════════════════════════════════════════════════════════════════ */

size_t uft_export_golden_count(void) {
    return otdr12_golden_count();
}

int uft_export_golden_run(size_t idx) {
    if (idx >= otdr12_golden_count()) return -1;

    const otdr12_golden_info_t *info = otdr12_golden_info(idx);
    if (!info) return -1;

    float *signal = (float *)malloc(info->n * sizeof(float));
    if (!signal) return -2;

    if (otdr12_golden_generate(idx, signal, info->n) != 0) {
        free(signal);
        return -3;
    }

    otdr12_result_t result;
    if (otdr12_analyze(signal, info->n, &result) != 0) {
        free(signal);
        return -4;
    }

    int rc = otdr12_golden_validate(idx, &result);
    otdr12_free_result(&result);
    free(signal);
    return rc;
}

int uft_export_golden_run_all(void) {
    size_t n = otdr12_golden_count();
    for (size_t i = 0; i < n; i++) {
        int rc = uft_export_golden_run(i);
        if (rc != 0) return (int)(i + 1) * 100 + rc;
    }
    return 0;
}

/* ════════════════════════════════════════════════════════════════════
 * Version / Utilities
 * ════════════════════════════════════════════════════════════════════ */

const char *uft_export_version(void) { return V12_BRIDGE_VERSION; }
const char *uft_export_pipeline_version(void) { return otdr12_full_version(); }
size_t uft_export_module_count(void) { return otdr12_module_count(); }

const char *uft_export_error_str(uft_export_error_t e) {
    switch (e) {
    case UFT_EXP_OK:           return "OK";
    case UFT_EXP_ERR_NULL:     return "NULL parameter";
    case UFT_EXP_ERR_NOMEM:    return "Out of memory";
    case UFT_EXP_ERR_SMALL:    return "Data too small";
    case UFT_EXP_ERR_FORMAT:   return "Invalid format";
    case UFT_EXP_ERR_INTERNAL: return "Internal error";
    default:                   return "Unknown error";
    }
}

const char *uft_export_format_str(uft_export_format_t f) {
    switch (f) {
    case UFT_EXPORT_JSON:   return "JSON";
    case UFT_EXPORT_CSV:    return "CSV";
    case UFT_EXPORT_BINARY: return "BINARY";
    default:                return "UNKNOWN";
    }
}
