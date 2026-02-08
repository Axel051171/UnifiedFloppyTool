/**
 * uft_event_bridge.c — UFT ↔ OTDR Event Detection Bridge
 * ========================================================
 *
 * Wraps otdr_event_core_v2 for UFT flux analysis.
 * Maps OTDR event types to floppy domain, adds quality scoring,
 * and provides flux-specific input conversions.
 */

#include "otdr_event_core_v2.h"
#include "uft_event_bridge.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#define EVT_BRIDGE_VERSION "1.0.0"
#define INITIAL_EVENT_CAP  256
#define MAX_SEGMENTS       8192

/* ════════════════════════════════════════════════════════════════════
 * Helpers
 * ════════════════════════════════════════════════════════════════════ */

static uft_event_type_t map_otdr_event(otdr_event_t e) {
    switch (e) {
    case OTDR_EVT_NONE:         return UFT_EVT_NORMAL;
    case OTDR_EVT_REFLECTION:   return UFT_EVT_SPIKE;
    case OTDR_EVT_ATTENUATION:  return UFT_EVT_DEGRADATION;
    case OTDR_EVT_REFLECT_LOSS: return UFT_EVT_COMPOUND;
    default:                    return UFT_EVT_NORMAL;
    }
}

static int ensure_event_capacity(uft_event_ctx_t *ctx, size_t needed) {
    if (needed <= ctx->event_capacity) return 0;
    size_t new_cap = ctx->event_capacity * 2;
    if (new_cap < needed) new_cap = needed;
    if (new_cap < INITIAL_EVENT_CAP) new_cap = INITIAL_EVENT_CAP;

    uft_event_info_t *p = (uft_event_info_t *)realloc(
        ctx->events, new_cap * sizeof(uft_event_info_t));
    if (!p) return -1;
    ctx->events = p;
    ctx->event_capacity = new_cap;
    return 0;
}

/* ════════════════════════════════════════════════════════════════════
 * Public: Config / Init / Free
 * ════════════════════════════════════════════════════════════════════ */

uft_event_config_t uft_event_default_config(void) {
    uft_event_config_t c;
    memset(&c, 0, sizeof(c));

    c.spike_snr_db    = 12.0f;
    c.degrad_snr_db   = 10.0f;
    c.min_signal_rms  = 1e-4f;

    c.local_sigma     = true;
    c.sigma_window    = 4096;
    c.sigma_stride    = 256;

    c.env_window      = 1025;

    c.enable_merge    = true;
    c.merge_gap       = 64;
    c.min_spike_len   = 1;
    c.min_degrad_len  = 2;

    c.min_event_len   = 1;
    c.min_confidence  = 0.0f;

    return c;
}

uft_event_error_t uft_event_init(uft_event_ctx_t *ctx,
                                  const uft_event_config_t *cfg) {
    if (!ctx) return UFT_EVT_ERR_NULL;
    memset(ctx, 0, sizeof(*ctx));

    ctx->cfg = cfg ? *cfg : uft_event_default_config();

    /* Validate */
    if (ctx->cfg.env_window < 3) ctx->cfg.env_window = 3;
    if (ctx->cfg.sigma_window < 16) ctx->cfg.sigma_window = 16;
    if (ctx->cfg.sigma_stride < 1) ctx->cfg.sigma_stride = 1;

    ctx->events = (uft_event_info_t *)calloc(INITIAL_EVENT_CAP,
                                              sizeof(uft_event_info_t));
    if (!ctx->events) return UFT_EVT_ERR_NOMEM;
    ctx->event_capacity = INITIAL_EVENT_CAP;

    ctx->initialized = true;
    return UFT_EVT_OK;
}

void uft_event_free(uft_event_ctx_t *ctx) {
    if (!ctx) return;
    free(ctx->events);
    memset(ctx, 0, sizeof(*ctx));
}

/* ════════════════════════════════════════════════════════════════════
 * Internal: core detection pipeline
 * ════════════════════════════════════════════════════════════════════ */

static uft_event_error_t detect_core(uft_event_ctx_t *ctx,
                                       const float *signal, size_t n) {
    if (!ctx || !ctx->initialized) return UFT_EVT_ERR_NULL;
    if (!signal) return UFT_EVT_ERR_NULL;
    if (n < 4) return UFT_EVT_ERR_SMALL;

    uft_event_config_t *cfg = &ctx->cfg;
    uft_event_report_t *rpt = &ctx->report;
    memset(rpt, 0, sizeof(*rpt));
    rpt->samples_analyzed = n;
    ctx->event_count = 0;

    /* 1) Build otdr_config */
    otdr_config ocfg = otdr_default_config();
    ocfg.window              = cfg->env_window;
    ocfg.thr_reflect_snr_db  = cfg->spike_snr_db;
    ocfg.thr_atten_snr_db    = cfg->degrad_snr_db;
    ocfg.min_env_rms         = cfg->min_signal_rms;
    ocfg.local_sigma_enable  = cfg->local_sigma ? 1 : 0;
    ocfg.sigma_window        = cfg->sigma_window;
    ocfg.sigma_stride        = cfg->sigma_stride;

    /* 2) Allocate feature/result arrays */
    otdr_features_t *feat = (otdr_features_t *)calloc(n, sizeof(otdr_features_t));
    otdr_event_result_t *res = (otdr_event_result_t *)calloc(n, sizeof(otdr_event_result_t));
    if (!feat || !res) {
        free(feat); free(res);
        return UFT_EVT_ERR_NOMEM;
    }

    /* 3) Run detection */
    int rc = otdr_detect_events_baseline(signal, n, &ocfg, feat, res);
    if (rc != 0) {
        free(feat); free(res);
        return UFT_EVT_ERR_INTERNAL;
    }

    /* 4) Collect noise statistics from features */
    double sigma_sum = 0.0;
    float sigma_max = 0.0f;
    double snr_sum = 0.0;
    float worst_snr = 100.0f;

    for (size_t i = 0; i < n; i++) {
        sigma_sum += (double)feat[i].noise_sigma;
        if (feat[i].noise_sigma > sigma_max)
            sigma_max = feat[i].noise_sigma;
        snr_sum += (double)feat[i].snr_db;
    }
    rpt->sigma_mean = (float)(sigma_sum / (double)n);
    rpt->sigma_max  = sigma_max;
    rpt->mean_snr_db = (float)(snr_sum / (double)n);

    /* 5) Segment with merge */
    otdr_segment_t *segs = (otdr_segment_t *)calloc(MAX_SEGMENTS,
                                                     sizeof(otdr_segment_t));
    if (!segs) {
        free(feat); free(res);
        return UFT_EVT_ERR_NOMEM;
    }

    size_t nseg;
    if (cfg->enable_merge) {
        otdr_merge_config mc = otdr_default_merge_config();
        mc.merge_gap_max       = cfg->merge_gap;
        mc.min_reflection_len  = cfg->min_spike_len;
        mc.min_atten_len       = cfg->min_degrad_len;
        nseg = otdr_rle_segments_merged(res, n, &mc, segs, MAX_SEGMENTS);
    } else {
        nseg = otdr_rle_segments(res, n, segs, MAX_SEGMENTS);
    }

    /* 6) Convert segments to UFT events (skip NORMAL segments) */
    size_t affected_samples = 0;

    for (size_t i = 0; i < nseg; i++) {
        uft_event_type_t type = map_otdr_event(segs[i].label);
        if (type == UFT_EVT_NORMAL) continue;

        size_t len = segs[i].end - segs[i].start + 1;

        /* Apply filters */
        if (len < cfg->min_event_len) continue;
        if (segs[i].mean_conf < cfg->min_confidence) continue;

        /* Compute event metrics from features */
        float peak_delta = 0.0f;
        float snr_acc = 0.0f;

        for (size_t j = segs[i].start; j <= segs[i].end && j < n; j++) {
            float ad = fabsf(feat[j].delta);
            if (ad > peak_delta) peak_delta = ad;
            snr_acc += feat[j].snr_db;
            if (feat[j].snr_db < worst_snr) worst_snr = feat[j].snr_db;
        }

        float snr_mean = snr_acc / (float)len;

        /* Severity: based on peak delta relative to local sigma */
        float local_sigma = feat[segs[i].start].noise_sigma;
        float severity = 0.0f;
        if (local_sigma > 0.0f) {
            severity = peak_delta / (local_sigma * 20.0f);
            if (severity > 1.0f) severity = 1.0f;
        }

        /* Store event */
        if (ensure_event_capacity(ctx, ctx->event_count + 1) != 0) {
            free(feat); free(res); free(segs);
            return UFT_EVT_ERR_NOMEM;
        }

        uft_event_info_t *evt = &ctx->events[ctx->event_count];
        evt->type       = type;
        evt->start      = segs[i].start;
        evt->end        = segs[i].end;
        evt->length     = len;
        evt->confidence = segs[i].mean_conf;
        evt->severity   = severity;
        evt->snr_mean_db = snr_mean;
        evt->amplitude  = peak_delta;
        evt->is_merged  = (segs[i].flags & OTDR_SEG_FLAG_MERGED) != 0;

        /* Count by type */
        switch (type) {
        case UFT_EVT_SPIKE:       rpt->spike_count++; break;
        case UFT_EVT_DEGRADATION: rpt->degradation_count++; break;
        case UFT_EVT_COMPOUND:    rpt->compound_count++; break;
        case UFT_EVT_WEAK_ZONE:   rpt->weak_zone_count++; break;
        default: break;
        }

        affected_samples += len;
        ctx->event_count++;
    }

    /* 7) Compute report */
    rpt->total_events      = ctx->event_count;
    rpt->event_density     = (n > 0)
        ? (float)ctx->event_count * 1000.0f / (float)n : 0.0f;
    rpt->affected_fraction = (n > 0)
        ? (float)affected_samples / (float)n : 0.0f;
    rpt->worst_snr_db      = worst_snr;

    /* Quality score: heuristic combining event density and affected fraction */
    float density_penalty  = 1.0f - (rpt->event_density / 100.0f);
    if (density_penalty < 0.0f) density_penalty = 0.0f;
    float coverage_penalty = 1.0f - rpt->affected_fraction * 2.0f;
    if (coverage_penalty < 0.0f) coverage_penalty = 0.0f;
    rpt->quality_score = density_penalty * coverage_penalty;
    if (rpt->quality_score > 1.0f) rpt->quality_score = 1.0f;
    if (rpt->quality_score < 0.0f) rpt->quality_score = 0.0f;

    free(feat);
    free(res);
    free(segs);
    return UFT_EVT_OK;
}

/* ════════════════════════════════════════════════════════════════════
 * Public: Detection
 * ════════════════════════════════════════════════════════════════════ */

uft_event_error_t uft_event_detect_float(uft_event_ctx_t *ctx,
                                           const float *signal,
                                           size_t count) {
    return detect_core(ctx, signal, count);
}

uft_event_error_t uft_event_detect_flux_ns(uft_event_ctx_t *ctx,
                                             const uint32_t *flux_ns,
                                             size_t count) {
    if (!flux_ns) return UFT_EVT_ERR_NULL;
    if (count < 4) return UFT_EVT_ERR_SMALL;

    float *f = (float *)malloc(count * sizeof(float));
    if (!f) return UFT_EVT_ERR_NOMEM;
    for (size_t i = 0; i < count; i++) f[i] = (float)flux_ns[i];

    uft_event_error_t rc = detect_core(ctx, f, count);
    free(f);
    return rc;
}

uft_event_error_t uft_event_detect_analog(uft_event_ctx_t *ctx,
                                            const int16_t *samples,
                                            size_t count) {
    if (!samples) return UFT_EVT_ERR_NULL;
    if (count < 4) return UFT_EVT_ERR_SMALL;

    float *f = (float *)malloc(count * sizeof(float));
    if (!f) return UFT_EVT_ERR_NOMEM;
    for (size_t i = 0; i < count; i++) f[i] = (float)samples[i] / 32768.0f;

    uft_event_error_t rc = detect_core(ctx, f, count);
    free(f);
    return rc;
}

/* ════════════════════════════════════════════════════════════════════
 * Public: Results / Utility
 * ════════════════════════════════════════════════════════════════════ */

size_t uft_event_count(const uft_event_ctx_t *ctx) {
    return ctx ? ctx->event_count : 0;
}

const uft_event_info_t *uft_event_get(const uft_event_ctx_t *ctx, size_t idx) {
    if (!ctx || idx >= ctx->event_count) return NULL;
    return &ctx->events[idx];
}

uft_event_report_t uft_event_get_report(const uft_event_ctx_t *ctx) {
    uft_event_report_t empty;
    memset(&empty, 0, sizeof(empty));
    if (!ctx) return empty;
    return ctx->report;
}

const char *uft_event_type_str(uft_event_type_t type) {
    switch (type) {
    case UFT_EVT_NORMAL:      return "NORMAL";
    case UFT_EVT_SPIKE:       return "SPIKE";
    case UFT_EVT_DEGRADATION: return "DEGRADATION";
    case UFT_EVT_COMPOUND:    return "COMPOUND";
    case UFT_EVT_WEAK_ZONE:   return "WEAK_ZONE";
    default:                  return "UNKNOWN";
    }
}

const char *uft_event_error_str(uft_event_error_t err) {
    switch (err) {
    case UFT_EVT_OK:           return "OK";
    case UFT_EVT_ERR_NULL:     return "NULL parameter";
    case UFT_EVT_ERR_NOMEM:    return "Out of memory";
    case UFT_EVT_ERR_SMALL:    return "Data too small";
    case UFT_EVT_ERR_CONFIG:   return "Invalid configuration";
    case UFT_EVT_ERR_INTERNAL: return "Internal detection error";
    default:                   return "Unknown error";
    }
}

const char *uft_event_version(void) {
    return EVT_BRIDGE_VERSION;
}
