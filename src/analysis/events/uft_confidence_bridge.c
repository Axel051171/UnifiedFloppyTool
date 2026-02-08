/**
 * uft_confidence_bridge.c — UFT ↔ OTDR v10 Confidence Map Bridge
 * ================================================================
 */

#include "otdr_event_core_v10.h"
#include "uft_confidence_bridge.h"

#include <stdlib.h>
#include <string.h>

#define V10_BRIDGE_VERSION "1.0.0"
#define INITIAL_SEG_CAP    256
#define MAX_SEGMENTS       8192

/* ════════════════════════════════════════════════════════════════════
 * Config / Init / Free
 * ════════════════════════════════════════════════════════════════════ */

uft_conf_config_t uft_conf_default_config(void) {
    uft_conf_config_t c;
    memset(&c, 0, sizeof(c));
    c.w_agreement       = 0.40f;
    c.w_snr             = 0.35f;
    c.w_integrity       = 0.25f;
    c.snr_floor_db      = -10.0f;
    c.snr_ceil_db       =  40.0f;
    c.integ_clean       = 1.0f;
    c.integ_flagged     = 0.0f;
    c.integ_repaired    = 0.30f;
    c.min_segment_len   = 16;
    c.default_agreement = 0.5f;
    c.default_snr_db    = 10.0f;
    return c;
}

uft_conf_error_t uft_conf_init(uft_conf_ctx_t *ctx, const uft_conf_config_t *cfg) {
    if (!ctx) return UFT_CONF_ERR_NULL;
    memset(ctx, 0, sizeof(*ctx));
    ctx->cfg = cfg ? *cfg : uft_conf_default_config();

    ctx->segments = calloc(INITIAL_SEG_CAP, sizeof(uft_conf_segment_t));
    if (!ctx->segments) return UFT_CONF_ERR_NOMEM;
    ctx->segment_capacity = INITIAL_SEG_CAP;
    ctx->initialized = true;
    return UFT_CONF_OK;
}

void uft_conf_free(uft_conf_ctx_t *ctx) {
    if (!ctx) return;
    free(ctx->samples);
    free(ctx->segments);
    memset(ctx, 0, sizeof(*ctx));
}

/* ════════════════════════════════════════════════════════════════════
 * Compute
 * ════════════════════════════════════════════════════════════════════ */

uft_conf_error_t uft_conf_compute(uft_conf_ctx_t *ctx,
                                    const float *agreement,
                                    const float *snr_db,
                                    const uint8_t *flags,
                                    size_t n) {
    if (!ctx || !ctx->initialized) return UFT_CONF_ERR_NULL;
    if (n < 2) return UFT_CONF_ERR_SMALL;

    /* Allocate sample output */
    free(ctx->samples);
    ctx->samples = calloc(n, sizeof(uft_conf_sample_t));
    if (!ctx->samples) return UFT_CONF_ERR_NOMEM;
    ctx->sample_count = n;

    /* Build v10 config from bridge config */
    otdr10_config_t oc = otdr10_default_config();
    oc.w_agreement       = ctx->cfg.w_agreement;
    oc.w_snr             = ctx->cfg.w_snr;
    oc.w_integrity       = ctx->cfg.w_integrity;
    oc.snr_floor_db      = ctx->cfg.snr_floor_db;
    oc.snr_ceil_db       = ctx->cfg.snr_ceil_db;
    oc.integ_clean       = ctx->cfg.integ_clean;
    oc.integ_flagged     = ctx->cfg.integ_flagged;
    oc.integ_repaired    = ctx->cfg.integ_repaired;
    oc.min_segment_len   = ctx->cfg.min_segment_len;
    oc.default_agreement = ctx->cfg.default_agreement;
    oc.default_snr_db    = ctx->cfg.default_snr_db;

    /* Compute per-sample confidence via v10 core */
    otdr10_sample_t *raw = calloc(n, sizeof(otdr10_sample_t));
    if (!raw) return UFT_CONF_ERR_NOMEM;

    int rc = otdr10_compute(agreement, snr_db, flags, n, &oc, raw);
    if (rc != 0) { free(raw); return UFT_CONF_ERR_INTERNAL; }

    /* Convert to bridge output + assign bands */
    for (size_t i = 0; i < n; i++) {
        ctx->samples[i].confidence = raw[i].confidence;
        ctx->samples[i].agree_comp = raw[i].agree_comp;
        ctx->samples[i].snr_comp   = raw[i].snr_comp;
        ctx->samples[i].integ_comp = raw[i].integ_comp;

        if (raw[i].confidence >= 0.8f)
            ctx->samples[i].band = UFT_CONF_HIGH;
        else if (raw[i].confidence >= 0.4f)
            ctx->samples[i].band = UFT_CONF_MID;
        else
            ctx->samples[i].band = UFT_CONF_LOW;
    }

    /* Segment + rank via v10 core */
    otdr10_segment_t *raw_seg = calloc(MAX_SEGMENTS, sizeof(otdr10_segment_t));
    if (!raw_seg) { free(raw); return UFT_CONF_ERR_NOMEM; }

    size_t nseg = otdr10_segment_rank(raw, n, &oc, raw_seg, MAX_SEGMENTS);

    /* Convert segments */
    if (nseg > ctx->segment_capacity) {
        uft_conf_segment_t *p = realloc(ctx->segments,
            nseg * sizeof(uft_conf_segment_t));
        if (!p) { free(raw); free(raw_seg); return UFT_CONF_ERR_NOMEM; }
        ctx->segments = p;
        ctx->segment_capacity = nseg;
    }
    ctx->segment_count = nseg;

    for (size_t i = 0; i < nseg; i++) {
        uft_conf_segment_t *s = &ctx->segments[i];
        s->start           = raw_seg[i].start;
        s->end             = raw_seg[i].end;
        s->length          = raw_seg[i].end - raw_seg[i].start + 1;
        s->mean_confidence = raw_seg[i].mean_confidence;
        s->min_confidence  = raw_seg[i].min_confidence;
        s->rank            = raw_seg[i].rank;
        s->flagged_count   = raw_seg[i].flagged_count;

        if (s->mean_confidence >= 0.8f)      s->band = UFT_CONF_HIGH;
        else if (s->mean_confidence >= 0.4f) s->band = UFT_CONF_MID;
        else                                 s->band = UFT_CONF_LOW;
    }

    /* Summary via v10 core */
    otdr10_summary_t summ;
    otdr10_summarize(raw, n, raw_seg, nseg, &summ);

    uft_conf_report_t *rpt = &ctx->report;
    memset(rpt, 0, sizeof(*rpt));
    rpt->samples_analyzed   = n;
    rpt->mean_confidence    = summ.mean_confidence;
    rpt->median_confidence  = summ.median_confidence;
    rpt->min_confidence     = summ.min_confidence;
    rpt->max_confidence     = summ.max_confidence;
    rpt->high_count         = summ.high_conf_count;
    rpt->mid_count          = summ.mid_conf_count;
    rpt->low_count          = summ.low_conf_count;
    rpt->high_fraction      = summ.high_conf_frac;
    rpt->low_fraction       = summ.low_conf_frac;
    rpt->num_segments       = nseg;
    rpt->overall_quality    = summ.overall_quality;

    free(raw);
    free(raw_seg);
    return UFT_CONF_OK;
}

/* ════════════════════════════════════════════════════════════════════
 * Results
 * ════════════════════════════════════════════════════════════════════ */

size_t uft_conf_sample_count(const uft_conf_ctx_t *ctx) {
    return ctx ? ctx->sample_count : 0;
}

const uft_conf_sample_t *uft_conf_get_sample(const uft_conf_ctx_t *ctx, size_t idx) {
    if (!ctx || idx >= ctx->sample_count) return NULL;
    return &ctx->samples[idx];
}

size_t uft_conf_segment_count(const uft_conf_ctx_t *ctx) {
    return ctx ? ctx->segment_count : 0;
}

const uft_conf_segment_t *uft_conf_get_segment(const uft_conf_ctx_t *ctx, size_t idx) {
    if (!ctx || idx >= ctx->segment_count) return NULL;
    return &ctx->segments[idx];
}

uft_conf_report_t uft_conf_get_report(const uft_conf_ctx_t *ctx) {
    uft_conf_report_t empty;
    memset(&empty, 0, sizeof(empty));
    return ctx ? ctx->report : empty;
}

size_t uft_conf_count_band(const uft_conf_ctx_t *ctx, uft_conf_band_t band) {
    if (!ctx) return 0;
    size_t count = 0;
    for (size_t i = 0; i < ctx->sample_count; i++)
        if (ctx->samples[i].band == band) count++;
    return count;
}

/* ════════════════════════════════════════════════════════════════════
 * Utilities
 * ════════════════════════════════════════════════════════════════════ */

const char *uft_conf_band_str(uft_conf_band_t b) {
    switch (b) {
    case UFT_CONF_HIGH: return "HIGH";
    case UFT_CONF_MID:  return "MID";
    case UFT_CONF_LOW:  return "LOW";
    default:            return "UNKNOWN";
    }
}

const char *uft_conf_error_str(uft_conf_error_t e) {
    switch (e) {
    case UFT_CONF_OK:           return "OK";
    case UFT_CONF_ERR_NULL:     return "NULL parameter";
    case UFT_CONF_ERR_NOMEM:    return "Out of memory";
    case UFT_CONF_ERR_SMALL:    return "Data too small";
    case UFT_CONF_ERR_INTERNAL: return "Internal error";
    default:                    return "Unknown error";
    }
}

const char *uft_conf_version(void) { return V10_BRIDGE_VERSION; }
