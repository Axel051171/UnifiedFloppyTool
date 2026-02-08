/**
 * uft_event_v8_bridge.c — UFT ↔ OTDR Event v8 Bridge
 * =====================================================
 */

#include "otdr_event_core_v8.h"
#include "uft_event_v8_bridge.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#define V8_BRIDGE_VERSION "1.0.0"
#define INITIAL_CAP       256
#define MAX_SEGMENTS      16384

/* ════════════════════════════════════════════════════════════════════
 * Helpers
 * ════════════════════════════════════════════════════════════════════ */

static uft_ev8_type_t map_event(otdr8_event_t e) {
    switch (e) {
    case OTDR8_EVT_NONE:         return UFT_EV8_NORMAL;
    case OTDR8_EVT_REFLECTION:   return UFT_EV8_SPIKE;
    case OTDR8_EVT_ATTENUATION:  return UFT_EV8_DEGRADATION;
    case OTDR8_EVT_REFLECT_LOSS: return UFT_EV8_COMPOUND;
    case OTDR8_EVT_GAINUP:       return UFT_EV8_RECOVERY;
    case OTDR8_EVT_SPIKE_NEG:    return UFT_EV8_DROPOUT;
    case OTDR8_EVT_OSCILLATION:  return UFT_EV8_FLUTTER;
    case OTDR8_EVT_BROADLOSS:    return UFT_EV8_WEAKSIGNAL;
    default:                     return UFT_EV8_NORMAL;
    }
}

static uft_ev8_verdict_t map_verdict(otdr8_verdict_t v) {
    switch (v) {
    case OTDR8_VERDICT_PASS: return UFT_EV8_PASS;
    case OTDR8_VERDICT_WARN: return UFT_EV8_WARN;
    case OTDR8_VERDICT_FAIL: return UFT_EV8_FAIL;
    default:                 return UFT_EV8_PASS;
    }
}

static int ensure_cap(uft_ev8_ctx_t *ctx, size_t needed) {
    if (needed <= ctx->event_capacity) return 0;
    size_t cap = ctx->event_capacity * 2;
    if (cap < needed) cap = needed;
    if (cap < INITIAL_CAP) cap = INITIAL_CAP;
    uft_ev8_event_t *p = realloc(ctx->events, cap * sizeof(uft_ev8_event_t));
    if (!p) return -1;
    ctx->events = p;
    ctx->event_capacity = cap;
    return 0;
}

/* ════════════════════════════════════════════════════════════════════
 * Config / Init / Free
 * ════════════════════════════════════════════════════════════════════ */

uft_ev8_config_t uft_ev8_default_config(void) {
    uft_ev8_config_t c;
    memset(&c, 0, sizeof(c));

    c.scale_windows[0] = 128;
    c.scale_windows[1] = 512;
    c.scale_windows[2] = 2048;
    c.scale_windows[3] = 8192;
    c.num_scales = 4;

    c.spike_snr_db     = 12.0f;
    c.degrad_snr_db    = 10.0f;
    c.dropout_snr_db   = 12.0f;
    c.flutter_snr_db   = 8.0f;
    c.broadloss_snr_db = 6.0f;
    c.min_signal_rms   = 1e-4f;

    c.local_sigma   = true;
    c.sigma_window  = 4096;
    c.sigma_stride  = 256;

    c.enable_merge    = true;
    c.iterative_merge = true;

    c.enable_passfail        = true;
    c.pf_max_loss_db         = 1.0f;
    c.pf_max_reflectance_db  = 35.0f;
    c.pf_min_snr_db          = 6.0f;
    c.pf_max_event_length    = 500;
    c.pf_warn_factor         = 0.7f;

    c.min_event_len  = 1;
    c.min_confidence = 0.0f;

    return c;
}

uft_ev8_error_t uft_ev8_init(uft_ev8_ctx_t *ctx, const uft_ev8_config_t *cfg) {
    if (!ctx) return UFT_EV8_ERR_NULL;
    memset(ctx, 0, sizeof(*ctx));
    ctx->cfg = cfg ? *cfg : uft_ev8_default_config();

    ctx->events = calloc(INITIAL_CAP, sizeof(uft_ev8_event_t));
    if (!ctx->events) return UFT_EV8_ERR_NOMEM;
    ctx->event_capacity = INITIAL_CAP;
    ctx->initialized = true;
    return UFT_EV8_OK;
}

void uft_ev8_free(uft_ev8_ctx_t *ctx) {
    if (!ctx) return;
    free(ctx->events);
    memset(ctx, 0, sizeof(*ctx));
}

/* ════════════════════════════════════════════════════════════════════
 * Core detection
 * ════════════════════════════════════════════════════════════════════ */

static uft_ev8_error_t detect_core(uft_ev8_ctx_t *ctx,
                                     const float *sig, size_t n) {
    if (!ctx || !ctx->initialized) return UFT_EV8_ERR_NULL;
    if (!sig) return UFT_EV8_ERR_NULL;
    if (n < 8) return UFT_EV8_ERR_SMALL;

    uft_ev8_config_t *cfg = &ctx->cfg;
    uft_ev8_report_t *rpt = &ctx->report;
    memset(rpt, 0, sizeof(*rpt));
    rpt->samples_analyzed = n;
    ctx->event_count = 0;

    /* Build v8 config */
    otdr8_config_t oc = otdr8_default_config();
    for (size_t s = 0; s < cfg->num_scales && s < OTDR_V8_MAX_SCALES; s++)
        oc.scale_windows[s] = cfg->scale_windows[s];
    oc.num_scales            = cfg->num_scales;
    oc.thr_reflect_snr_db    = cfg->spike_snr_db;
    oc.thr_atten_snr_db      = cfg->degrad_snr_db;
    oc.thr_spike_neg_snr_db  = cfg->dropout_snr_db;
    oc.thr_oscillation_snr_db= cfg->flutter_snr_db;
    oc.thr_broadloss_snr_db  = cfg->broadloss_snr_db;
    oc.min_env_rms           = cfg->min_signal_rms;
    oc.local_sigma_enable    = cfg->local_sigma ? 1 : 0;
    oc.sigma_window          = cfg->sigma_window;
    oc.sigma_stride          = cfg->sigma_stride;

    /* Detect */
    otdr8_features_t *feat = calloc(n, sizeof(otdr8_features_t));
    otdr8_result_t   *res  = calloc(n, sizeof(otdr8_result_t));
    if (!feat || !res) { free(feat); free(res); return UFT_EV8_ERR_NOMEM; }

    int rc = otdr8_detect(sig, n, &oc, feat, res);
    if (rc != 0) { free(feat); free(res); return UFT_EV8_ERR_INTERNAL; }

    /* Noise stats */
    double sigma_sum = 0.0, snr_sum = 0.0;
    for (size_t i = 0; i < n; i++) {
        sigma_sum += (double)feat[i].noise_sigma;
        snr_sum   += (double)feat[i].max_snr_db;
    }
    rpt->sigma_mean  = (float)(sigma_sum / (double)n);
    rpt->mean_snr_db = (float)(snr_sum / (double)n);

    /* Segment + merge */
    otdr8_segment_t *segs = calloc(MAX_SEGMENTS, sizeof(otdr8_segment_t));
    if (!segs) { free(feat); free(res); return UFT_EV8_ERR_NOMEM; }

    otdr8_merge_config_t mc = otdr8_default_merge_config();
    mc.iterative = cfg->iterative_merge ? 1 : 0;
    size_t nseg = cfg->enable_merge
        ? otdr8_segment_merge(res, feat, n, &mc, segs, MAX_SEGMENTS)
        : otdr8_segment_merge(res, feat, n, NULL, segs, MAX_SEGMENTS);

    /* Pass/fail */
    if (cfg->enable_passfail) {
        otdr8_passfail_config_t pf = otdr8_default_passfail_config();
        pf.max_loss_db        = cfg->pf_max_loss_db;
        pf.max_reflectance_db = cfg->pf_max_reflectance_db;
        pf.min_snr_db         = cfg->pf_min_snr_db;
        pf.max_event_length   = cfg->pf_max_event_length;
        pf.warn_factor        = cfg->pf_warn_factor;
        otdr8_apply_passfail(segs, nseg, &pf);
    }

    /* Convert to UFT events */
    size_t affected = 0;
    for (size_t i = 0; i < nseg; i++) {
        uft_ev8_type_t type = map_event(segs[i].label);
        if (type == UFT_EV8_NORMAL) continue;

        size_t len = segs[i].end - segs[i].start + 1;
        if (len < cfg->min_event_len) continue;
        if (segs[i].mean_conf < cfg->min_confidence) continue;

        if (ensure_cap(ctx, ctx->event_count + 1) != 0) {
            free(feat); free(res); free(segs);
            return UFT_EV8_ERR_NOMEM;
        }

        uft_ev8_event_t *ev = &ctx->events[ctx->event_count];
        ev->type           = type;
        ev->start          = segs[i].start;
        ev->end            = segs[i].end;
        ev->length         = len;
        ev->confidence     = segs[i].mean_conf;
        ev->peak_snr_db    = segs[i].peak_snr_db;
        ev->peak_amplitude = segs[i].peak_amplitude;
        ev->dominant_scale = 0;  /* could extract from features */
        ev->is_merged      = (segs[i].flags != 0);
        ev->verdict        = map_verdict(segs[i].verdict);
        ev->fail_reasons   = segs[i].fail_reasons;

        /* Severity: peak amplitude relative to sigma */
        float local_sig = (feat[segs[i].start].noise_sigma > 0.0f)
                          ? feat[segs[i].start].noise_sigma : 1e-6f;
        ev->severity = segs[i].peak_amplitude / (local_sig * 20.0f);
        if (ev->severity > 1.0f) ev->severity = 1.0f;

        /* Count by type */
        switch (type) {
        case UFT_EV8_SPIKE:       rpt->spike_count++; break;
        case UFT_EV8_DEGRADATION: rpt->degradation_count++; break;
        case UFT_EV8_COMPOUND:    rpt->compound_count++; break;
        case UFT_EV8_RECOVERY:    rpt->recovery_count++; break;
        case UFT_EV8_DROPOUT:     rpt->dropout_count++; break;
        case UFT_EV8_FLUTTER:     rpt->flutter_count++; break;
        case UFT_EV8_WEAKSIGNAL:  rpt->weaksignal_count++; break;
        default: break;
        }

        /* Verdicts */
        switch (ev->verdict) {
        case UFT_EV8_PASS: rpt->pass_count++; break;
        case UFT_EV8_WARN: rpt->warn_count++; break;
        case UFT_EV8_FAIL: rpt->fail_count++; break;
        }

        affected += len;
        ctx->event_count++;
    }

    rpt->total_events      = ctx->event_count;
    rpt->event_density     = (n > 0)
        ? (float)ctx->event_count * 1000.0f / (float)n : 0.0f;
    rpt->affected_fraction = (n > 0)
        ? (float)affected / (float)n : 0.0f;

    float dp = 1.0f - (rpt->event_density / 100.0f);
    if (dp < 0.0f) dp = 0.0f;
    float cp = 1.0f - rpt->affected_fraction * 2.0f;
    if (cp < 0.0f) cp = 0.0f;
    float fp = (rpt->total_events > 0)
        ? 1.0f - (float)rpt->fail_count / (float)rpt->total_events : 1.0f;
    rpt->quality_score = dp * cp * fp;
    if (rpt->quality_score < 0.0f) rpt->quality_score = 0.0f;
    if (rpt->quality_score > 1.0f) rpt->quality_score = 1.0f;

    free(feat); free(res); free(segs);
    return UFT_EV8_OK;
}

/* ════════════════════════════════════════════════════════════════════
 * Public detection
 * ════════════════════════════════════════════════════════════════════ */

uft_ev8_error_t uft_ev8_detect_float(uft_ev8_ctx_t *ctx,
                                       const float *signal, size_t n) {
    return detect_core(ctx, signal, n);
}

uft_ev8_error_t uft_ev8_detect_flux_ns(uft_ev8_ctx_t *ctx,
                                         const uint32_t *flux, size_t n) {
    if (!flux) return UFT_EV8_ERR_NULL;
    if (n < 8) return UFT_EV8_ERR_SMALL;
    float *f = malloc(n * sizeof(float));
    if (!f) return UFT_EV8_ERR_NOMEM;
    for (size_t i = 0; i < n; i++) f[i] = (float)flux[i];
    uft_ev8_error_t rc = detect_core(ctx, f, n);
    free(f);
    return rc;
}

uft_ev8_error_t uft_ev8_detect_analog(uft_ev8_ctx_t *ctx,
                                        const int16_t *samples, size_t n) {
    if (!samples) return UFT_EV8_ERR_NULL;
    if (n < 8) return UFT_EV8_ERR_SMALL;
    float *f = malloc(n * sizeof(float));
    if (!f) return UFT_EV8_ERR_NOMEM;
    for (size_t i = 0; i < n; i++) f[i] = (float)samples[i] / 32768.0f;
    uft_ev8_error_t rc = detect_core(ctx, f, n);
    free(f);
    return rc;
}

/* ════════════════════════════════════════════════════════════════════
 * Results
 * ════════════════════════════════════════════════════════════════════ */

size_t uft_ev8_count(const uft_ev8_ctx_t *ctx) {
    return ctx ? ctx->event_count : 0;
}

const uft_ev8_event_t *uft_ev8_get(const uft_ev8_ctx_t *ctx, size_t idx) {
    if (!ctx || idx >= ctx->event_count) return NULL;
    return &ctx->events[idx];
}

uft_ev8_report_t uft_ev8_get_report(const uft_ev8_ctx_t *ctx) {
    uft_ev8_report_t empty;
    memset(&empty, 0, sizeof(empty));
    return ctx ? ctx->report : empty;
}

size_t uft_ev8_count_by_verdict(const uft_ev8_ctx_t *ctx, uft_ev8_verdict_t v) {
    if (!ctx) return 0;
    size_t count = 0;
    for (size_t i = 0; i < ctx->event_count; i++)
        if (ctx->events[i].verdict == v) count++;
    return count;
}

const char *uft_ev8_type_str(uft_ev8_type_t t) {
    switch (t) {
    case UFT_EV8_NORMAL:      return "NORMAL";
    case UFT_EV8_SPIKE:       return "SPIKE";
    case UFT_EV8_DEGRADATION: return "DEGRADATION";
    case UFT_EV8_COMPOUND:    return "COMPOUND";
    case UFT_EV8_RECOVERY:    return "RECOVERY";
    case UFT_EV8_DROPOUT:     return "DROPOUT";
    case UFT_EV8_FLUTTER:     return "FLUTTER";
    case UFT_EV8_WEAKSIGNAL:  return "WEAKSIGNAL";
    default:                  return "UNKNOWN";
    }
}

const char *uft_ev8_verdict_str(uft_ev8_verdict_t v) {
    switch (v) {
    case UFT_EV8_PASS: return "PASS";
    case UFT_EV8_WARN: return "WARN";
    case UFT_EV8_FAIL: return "FAIL";
    default:           return "UNKNOWN";
    }
}

const char *uft_ev8_error_str(uft_ev8_error_t e) {
    switch (e) {
    case UFT_EV8_OK:           return "OK";
    case UFT_EV8_ERR_NULL:     return "NULL parameter";
    case UFT_EV8_ERR_NOMEM:    return "Out of memory";
    case UFT_EV8_ERR_SMALL:    return "Data too small";
    case UFT_EV8_ERR_CONFIG:   return "Invalid configuration";
    case UFT_EV8_ERR_INTERNAL: return "Internal detection error";
    default:                   return "Unknown error";
    }
}

const char *uft_ev8_version(void) { return V8_BRIDGE_VERSION; }
