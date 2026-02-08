/**
 * uft_integrity_bridge.c — UFT ↔ OTDR v9 Signal Integrity Bridge
 * =================================================================
 */

#include "otdr_event_core_v9.h"
#include "uft_integrity_bridge.h"

#include <stdlib.h>
#include <string.h>

#define V9_BRIDGE_VERSION "1.0.0"
#define INITIAL_CAP       128
#define MAX_REGIONS       4096

/* ════════════════════════════════════════════════════════════════════
 * Helpers
 * ════════════════════════════════════════════════════════════════════ */

static uft_integrity_type_t map_anomaly(otdr9_anomaly_t a) {
    switch (a) {
    case OTDR9_ANOM_DROPOUT:   return UFT_INT_DROPOUT;
    case OTDR9_ANOM_SATURATED: return UFT_INT_SATURATED;
    case OTDR9_ANOM_STUCK:     return UFT_INT_STUCK;
    case OTDR9_ANOM_DEADZONE:  return UFT_INT_DEADZONE;
    default:                   return UFT_INT_NORMAL;
    }
}

/* ════════════════════════════════════════════════════════════════════
 * Config / Init / Free
 * ════════════════════════════════════════════════════════════════════ */

uft_integrity_config_t uft_integrity_default_config(void) {
    uft_integrity_config_t c;
    memset(&c, 0, sizeof(c));
    c.dropout_threshold = 1e-4f;
    c.dropout_min_run   = 3;
    c.clip_high         = 0.99f;
    c.clip_low          = -0.99f;
    c.clip_min_run      = 2;
    c.clip_auto_detect  = false;
    c.stuck_max_delta   = 1e-6f;
    c.stuck_min_run     = 5;
    c.deadzone_snr_db   = 3.0f;
    c.deadzone_min_run  = 64;
    c.auto_repair       = false;
    c.mark_exclude      = true;
    return c;
}

uft_integrity_error_t uft_integrity_init(uft_integrity_ctx_t *ctx,
                                          const uft_integrity_config_t *cfg) {
    if (!ctx) return UFT_INT_ERR_NULL;
    memset(ctx, 0, sizeof(*ctx));
    ctx->cfg = cfg ? *cfg : uft_integrity_default_config();

    ctx->regions = calloc(INITIAL_CAP, sizeof(uft_integrity_region_t));
    if (!ctx->regions) return UFT_INT_ERR_NOMEM;
    ctx->region_capacity = INITIAL_CAP;
    ctx->initialized = true;
    return UFT_INT_OK;
}

void uft_integrity_free(uft_integrity_ctx_t *ctx) {
    if (!ctx) return;
    free(ctx->regions);
    free(ctx->flags);
    memset(ctx, 0, sizeof(*ctx));
}

/* ════════════════════════════════════════════════════════════════════
 * Core scan
 * ════════════════════════════════════════════════════════════════════ */

static uft_integrity_error_t scan_core(uft_integrity_ctx_t *ctx,
                                         const float *sig, size_t n) {
    if (!ctx || !ctx->initialized) return UFT_INT_ERR_NULL;
    if (!sig) return UFT_INT_ERR_NULL;
    if (n < 4) return UFT_INT_ERR_SMALL;

    /* Build v9 config */
    otdr9_config_t oc = otdr9_default_config();
    oc.dropout_threshold = ctx->cfg.dropout_threshold;
    oc.dropout_min_run   = ctx->cfg.dropout_min_run;
    oc.clip_high         = ctx->cfg.clip_high;
    oc.clip_low          = ctx->cfg.clip_low;
    oc.clip_min_run      = ctx->cfg.clip_min_run;
    oc.clip_auto_range   = ctx->cfg.clip_auto_detect ? 0.99f : 0.0f;
    oc.stuck_max_delta   = ctx->cfg.stuck_max_delta;
    oc.stuck_min_run     = ctx->cfg.stuck_min_run;
    oc.deadzone_snr_db   = ctx->cfg.deadzone_snr_db;
    oc.deadzone_min_run  = ctx->cfg.deadzone_min_run;
    oc.auto_repair       = 0;  /* we handle repair in bridge */
    oc.mark_exclude      = ctx->cfg.mark_exclude ? 1 : 0;

    /* Allocate flags + temp regions */
    free(ctx->flags);
    ctx->flags = calloc(n, sizeof(uint8_t));
    if (!ctx->flags) return UFT_INT_ERR_NOMEM;
    ctx->flags_len = n;

    otdr9_region_t *raw = calloc(MAX_REGIONS, sizeof(otdr9_region_t));
    if (!raw) return UFT_INT_ERR_NOMEM;

    otdr9_summary_t summ;
    int nreg = otdr9_scan(sig, n, &oc, ctx->flags, raw, MAX_REGIONS, &summ);
    if (nreg < 0) { free(raw); return UFT_INT_ERR_INTERNAL; }

    /* Convert regions */
    ctx->region_count = 0;
    if ((size_t)nreg > ctx->region_capacity) {
        uft_integrity_region_t *p = realloc(ctx->regions,
            (size_t)nreg * sizeof(uft_integrity_region_t));
        if (!p) { free(raw); return UFT_INT_ERR_NOMEM; }
        ctx->regions = p;
        ctx->region_capacity = (size_t)nreg;
    }

    for (int i = 0; i < nreg; i++) {
        uft_integrity_region_t *r = &ctx->regions[ctx->region_count];
        r->type        = map_anomaly(raw[i].type);
        r->start       = raw[i].start;
        r->end         = raw[i].end;
        r->length      = raw[i].end - raw[i].start + 1;
        r->severity    = raw[i].severity;
        r->mean_value  = raw[i].mean_value;
        r->stuck_value = raw[i].stuck_value;
        r->snr_db      = raw[i].snr_db;
        ctx->region_count++;
    }

    free(raw);

    /* Build report */
    uft_integrity_report_t *rpt = &ctx->report;
    memset(rpt, 0, sizeof(*rpt));
    rpt->samples_analyzed  = summ.samples_analyzed;
    rpt->total_regions     = summ.total_regions;
    rpt->dropout_count     = summ.dropout_count;
    rpt->saturated_count   = summ.saturated_count;
    rpt->stuck_count       = summ.stuck_count;
    rpt->deadzone_count    = summ.deadzone_count;
    rpt->dropout_samples   = summ.dropout_samples;
    rpt->saturated_samples = summ.saturated_samples;
    rpt->stuck_samples     = summ.stuck_samples;
    rpt->deadzone_samples  = summ.deadzone_samples;
    rpt->flagged_samples   = summ.flagged_samples;
    rpt->flagged_fraction  = summ.flagged_fraction;
    rpt->integrity_score   = summ.integrity_score;

    return UFT_INT_OK;
}

/* ════════════════════════════════════════════════════════════════════
 * Public: Scan
 * ════════════════════════════════════════════════════════════════════ */

uft_integrity_error_t uft_integrity_scan_float(uft_integrity_ctx_t *ctx,
                                                 const float *signal, size_t n) {
    return scan_core(ctx, signal, n);
}

uft_integrity_error_t uft_integrity_scan_flux_ns(uft_integrity_ctx_t *ctx,
                                                   const uint32_t *flux, size_t n) {
    if (!flux) return UFT_INT_ERR_NULL;
    if (n < 4) return UFT_INT_ERR_SMALL;
    float *f = malloc(n * sizeof(float));
    if (!f) return UFT_INT_ERR_NOMEM;
    for (size_t i = 0; i < n; i++) f[i] = (float)flux[i];
    uft_integrity_error_t rc = scan_core(ctx, f, n);
    free(f);
    return rc;
}

uft_integrity_error_t uft_integrity_scan_analog(uft_integrity_ctx_t *ctx,
                                                  const int16_t *samples, size_t n) {
    if (!samples) return UFT_INT_ERR_NULL;
    if (n < 4) return UFT_INT_ERR_SMALL;
    float *f = malloc(n * sizeof(float));
    if (!f) return UFT_INT_ERR_NOMEM;
    for (size_t i = 0; i < n; i++) f[i] = (float)samples[i] / 32768.0f;
    uft_integrity_error_t rc = scan_core(ctx, f, n);
    free(f);
    return rc;
}

/* ════════════════════════════════════════════════════════════════════
 * Public: Repair
 * ════════════════════════════════════════════════════════════════════ */

size_t uft_integrity_repair(uft_integrity_ctx_t *ctx, float *signal, size_t n) {
    if (!ctx || !ctx->initialized || !signal || !ctx->flags) return 0;
    if (n != ctx->flags_len) return 0;
    size_t count = otdr9_repair(signal, n, ctx->flags);
    ctx->report.repaired_samples = count;
    return count;
}

/* ════════════════════════════════════════════════════════════════════
 * Public: Results
 * ════════════════════════════════════════════════════════════════════ */

size_t uft_integrity_count(const uft_integrity_ctx_t *ctx) {
    return ctx ? ctx->region_count : 0;
}

const uft_integrity_region_t *uft_integrity_get(const uft_integrity_ctx_t *ctx,
                                                  size_t idx) {
    if (!ctx || idx >= ctx->region_count) return NULL;
    return &ctx->regions[idx];
}

const uint8_t *uft_integrity_flags(const uft_integrity_ctx_t *ctx,
                                    size_t *out_len) {
    if (!ctx || !ctx->flags) return NULL;
    if (out_len) *out_len = ctx->flags_len;
    return ctx->flags;
}

uft_integrity_report_t uft_integrity_get_report(const uft_integrity_ctx_t *ctx) {
    uft_integrity_report_t empty;
    memset(&empty, 0, sizeof(empty));
    return ctx ? ctx->report : empty;
}

/* ════════════════════════════════════════════════════════════════════
 * Utilities
 * ════════════════════════════════════════════════════════════════════ */

const char *uft_integrity_type_str(uft_integrity_type_t t) {
    switch (t) {
    case UFT_INT_NORMAL:    return "NORMAL";
    case UFT_INT_DROPOUT:   return "DROPOUT";
    case UFT_INT_SATURATED: return "SATURATED";
    case UFT_INT_STUCK:     return "STUCK";
    case UFT_INT_DEADZONE:  return "DEADZONE";
    default:                return "UNKNOWN";
    }
}

const char *uft_integrity_flag_str(uint8_t flag) {
    if (flag & UFT_INT_FLAG_DROPOUT)   return "DROPOUT";
    if (flag & UFT_INT_FLAG_CLIP_HIGH) return "CLIP_HIGH";
    if (flag & UFT_INT_FLAG_CLIP_LOW)  return "CLIP_LOW";
    if (flag & UFT_INT_FLAG_STUCK)     return "STUCK";
    if (flag & UFT_INT_FLAG_DEADZONE)  return "DEADZONE";
    if (flag & UFT_INT_FLAG_REPAIRED)  return "REPAIRED";
    if (flag & UFT_INT_FLAG_EXCLUDE)   return "EXCLUDE";
    return "OK";
}

const char *uft_integrity_error_str(uft_integrity_error_t e) {
    switch (e) {
    case UFT_INT_OK:           return "OK";
    case UFT_INT_ERR_NULL:     return "NULL parameter";
    case UFT_INT_ERR_NOMEM:    return "Out of memory";
    case UFT_INT_ERR_SMALL:    return "Data too small";
    case UFT_INT_ERR_INTERNAL: return "Internal scan error";
    default:                   return "Unknown error";
    }
}

const char *uft_integrity_version(void) { return V9_BRIDGE_VERSION; }
