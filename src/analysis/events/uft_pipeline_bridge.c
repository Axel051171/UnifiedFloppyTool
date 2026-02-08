/**
 * uft_pipeline_bridge.c — UFT ↔ OTDR v11 Pipeline/Streaming Bridge
 * ==================================================================
 */

#include "otdr_event_core_v11.h"
#include "uft_pipeline_bridge.h"

#include <stdlib.h>
#include <string.h>

#define V11_BRIDGE_VERSION "1.0.0"
#define MAX_CONVERT_BUF    32768

/* ════════════════════════════════════════════════════════════════════
 * Opaque context
 * ════════════════════════════════════════════════════════════════════ */

struct uft_pipe_ctx {
    otdr11_pipeline_t  pipeline;
    uft_pipe_config_t  cfg;

    /* Callback adapters: v11 → bridge format */
    uft_pipe_event_t  *event_buf;
    size_t             event_buf_cap;

    float             *convert_buf;  /* for flux/analog conversion */
    size_t             convert_cap;

    int initialized;
};

/* ════════════════════════════════════════════════════════════════════
 * Internal callback adapters
 * ════════════════════════════════════════════════════════════════════ */

static void chunk_adapter(const otdr11_chunk_result_t *result, void *ud) {
    uft_pipe_ctx_t *ctx = (uft_pipe_ctx_t *)ud;
    if (!ctx || !ctx->cfg.on_chunk) return;

    /* Convert v11 events → bridge events */
    size_t n = result->event_count;
    if (n > ctx->event_buf_cap) n = ctx->event_buf_cap;

    for (size_t i = 0; i < n; i++) {
        ctx->event_buf[i].abs_start  = result->events[i].abs_start;
        ctx->event_buf[i].abs_end    = result->events[i].abs_end;
        ctx->event_buf[i].length     = result->events[i].abs_end -
                                       result->events[i].abs_start + 1;
        ctx->event_buf[i].type       = result->events[i].type;
        ctx->event_buf[i].confidence = result->events[i].confidence;
        ctx->event_buf[i].severity   = result->events[i].severity;
        ctx->event_buf[i].flags      = result->events[i].flags;
    }

    uft_pipe_chunk_t chunk;
    memset(&chunk, 0, sizeof(chunk));
    chunk.chunk_id          = result->chunk_id;
    chunk.chunk_offset      = result->chunk_offset;
    chunk.chunk_len         = result->chunk_len;
    chunk.integrity_regions = result->integrity_regions;
    chunk.flagged_samples   = result->flagged_samples;
    chunk.integrity_score   = result->integrity_score;
    chunk.events            = ctx->event_buf;
    chunk.event_count       = n;
    chunk.mean_confidence   = result->mean_confidence;
    chunk.min_confidence    = result->min_confidence;

    ctx->cfg.on_chunk(&chunk, ctx->cfg.user_data);
}

static void event_adapter(const otdr11_event_t *event, void *ud) {
    uft_pipe_ctx_t *ctx = (uft_pipe_ctx_t *)ud;
    if (!ctx || !ctx->cfg.on_event) return;

    uft_pipe_event_t e;
    e.abs_start  = event->abs_start;
    e.abs_end    = event->abs_end;
    e.length     = event->abs_end - event->abs_start + 1;
    e.type       = event->type;
    e.confidence = event->confidence;
    e.severity   = event->severity;
    e.flags      = event->flags;

    ctx->cfg.on_event(&e, ctx->cfg.user_data);
}

/* ════════════════════════════════════════════════════════════════════
 * Config / Create / Destroy
 * ════════════════════════════════════════════════════════════════════ */

uft_pipe_config_t uft_pipe_default_config(void) {
    uft_pipe_config_t c;
    memset(&c, 0, sizeof(c));
    c.chunk_size        = 8192;
    c.overlap           = 256;
    c.ring_capacity     = 65536;
    c.enable_integrity  = true;
    c.enable_detect     = true;
    c.enable_confidence = true;
    c.auto_repair       = false;
    c.detect_threshold  = 12.0f;
    c.on_chunk = NULL;
    c.on_event = NULL;
    c.user_data = NULL;
    return c;
}

uft_pipe_error_t uft_pipe_create(uft_pipe_ctx_t **out, const uft_pipe_config_t *cfg) {
    if (!out) return UFT_PIPE_ERR_NULL;
    *out = NULL;

    uft_pipe_ctx_t *ctx = (uft_pipe_ctx_t *)calloc(1, sizeof(uft_pipe_ctx_t));
    if (!ctx) return UFT_PIPE_ERR_NOMEM;

    ctx->cfg = cfg ? *cfg : uft_pipe_default_config();

    /* Build v11 config */
    otdr11_config_t oc = otdr11_default_config();
    oc.chunk_size         = ctx->cfg.chunk_size;
    oc.overlap            = ctx->cfg.overlap;
    oc.ring_capacity      = ctx->cfg.ring_capacity;
    oc.enable_integrity   = ctx->cfg.enable_integrity ? 1 : 0;
    oc.enable_detect      = ctx->cfg.enable_detect ? 1 : 0;
    oc.enable_confidence  = ctx->cfg.enable_confidence ? 1 : 0;
    oc.auto_repair        = ctx->cfg.auto_repair ? 1 : 0;
    oc.detect_snr_threshold = ctx->cfg.detect_threshold;

    /* Hook adapters */
    oc.on_chunk   = ctx->cfg.on_chunk ? chunk_adapter : NULL;
    oc.on_event   = ctx->cfg.on_event ? event_adapter : NULL;
    oc.user_data  = ctx;  /* pass bridge ctx so adapters can access it */

    if (otdr11_init(&ctx->pipeline, &oc) != 0) {
        free(ctx);
        return UFT_PIPE_ERR_NOMEM;
    }

    ctx->event_buf_cap = OTDR11_MAX_EVENTS_PER_CHUNK;
    ctx->event_buf = (uft_pipe_event_t *)calloc(ctx->event_buf_cap,
                                                  sizeof(uft_pipe_event_t));
    ctx->convert_cap = MAX_CONVERT_BUF;
    ctx->convert_buf = (float *)malloc(ctx->convert_cap * sizeof(float));

    if (!ctx->event_buf || !ctx->convert_buf) {
        otdr11_free(&ctx->pipeline);
        free(ctx->event_buf);
        free(ctx->convert_buf);
        free(ctx);
        return UFT_PIPE_ERR_NOMEM;
    }

    ctx->initialized = 1;
    *out = ctx;
    return UFT_PIPE_OK;
}

void uft_pipe_destroy(uft_pipe_ctx_t *ctx) {
    if (!ctx) return;
    otdr11_free(&ctx->pipeline);
    free(ctx->event_buf);
    free(ctx->convert_buf);
    free(ctx);
}

/* ════════════════════════════════════════════════════════════════════
 * Push
 * ════════════════════════════════════════════════════════════════════ */

uft_pipe_error_t uft_pipe_push_float(uft_pipe_ctx_t *ctx,
                                       const float *samples, size_t n) {
    if (!ctx || !ctx->initialized) return UFT_PIPE_ERR_NULL;
    if (!samples) return UFT_PIPE_ERR_NULL;
    if (n == 0) return UFT_PIPE_OK;

    int rc = otdr11_push(&ctx->pipeline, samples, n);
    return (rc >= 0) ? UFT_PIPE_OK : UFT_PIPE_ERR_INTERNAL;
}

uft_pipe_error_t uft_pipe_push_flux_ns(uft_pipe_ctx_t *ctx,
                                         const uint32_t *flux, size_t n) {
    if (!ctx || !ctx->initialized || !flux) return UFT_PIPE_ERR_NULL;

    /* Convert in blocks */
    size_t off = 0;
    while (off < n) {
        size_t blk = n - off;
        if (blk > ctx->convert_cap) blk = ctx->convert_cap;
        for (size_t i = 0; i < blk; i++)
            ctx->convert_buf[i] = (float)flux[off + i];
        uft_pipe_error_t rc = uft_pipe_push_float(ctx, ctx->convert_buf, blk);
        if (rc != UFT_PIPE_OK) return rc;
        off += blk;
    }
    return UFT_PIPE_OK;
}

uft_pipe_error_t uft_pipe_push_analog(uft_pipe_ctx_t *ctx,
                                        const int16_t *samples, size_t n) {
    if (!ctx || !ctx->initialized || !samples) return UFT_PIPE_ERR_NULL;

    size_t off = 0;
    while (off < n) {
        size_t blk = n - off;
        if (blk > ctx->convert_cap) blk = ctx->convert_cap;
        for (size_t i = 0; i < blk; i++)
            ctx->convert_buf[i] = (float)samples[off + i] / 32768.0f;
        uft_pipe_error_t rc = uft_pipe_push_float(ctx, ctx->convert_buf, blk);
        if (rc != UFT_PIPE_OK) return rc;
        off += blk;
    }
    return UFT_PIPE_OK;
}

/* ════════════════════════════════════════════════════════════════════
 * Flush / Reset
 * ════════════════════════════════════════════════════════════════════ */

uft_pipe_error_t uft_pipe_flush(uft_pipe_ctx_t *ctx) {
    if (!ctx || !ctx->initialized) return UFT_PIPE_ERR_NULL;
    int rc = otdr11_flush(&ctx->pipeline);
    return (rc >= 0) ? UFT_PIPE_OK : UFT_PIPE_ERR_INTERNAL;
}

uft_pipe_error_t uft_pipe_reset(uft_pipe_ctx_t *ctx) {
    if (!ctx || !ctx->initialized) return UFT_PIPE_ERR_NULL;
    otdr11_reset(&ctx->pipeline);
    return UFT_PIPE_OK;
}

/* ════════════════════════════════════════════════════════════════════
 * Results
 * ════════════════════════════════════════════════════════════════════ */

uft_pipe_report_t uft_pipe_get_report(const uft_pipe_ctx_t *ctx) {
    uft_pipe_report_t r;
    memset(&r, 0, sizeof(r));
    if (!ctx) return r;

    otdr11_stats_t s = otdr11_get_stats(&ctx->pipeline);
    r.total_samples     = s.total_samples;
    r.chunks_processed  = s.chunks_processed;
    r.total_events      = s.total_events;
    r.total_flagged     = s.total_flagged;
    r.mean_integrity    = s.mean_integrity;
    r.mean_confidence   = s.mean_confidence;
    r.min_confidence    = s.min_confidence;
    r.is_done           = (s.state == OTDR11_STATE_DONE);

    /* Overall quality */
    r.overall_quality = 0.5f * s.mean_integrity + 0.5f * s.mean_confidence;
    if (r.overall_quality > 1.0f) r.overall_quality = 1.0f;

    return r;
}

uint32_t uft_pipe_chunks_processed(const uft_pipe_ctx_t *ctx) {
    if (!ctx) return 0;
    return otdr11_get_stats(&ctx->pipeline).chunks_processed;
}

size_t uft_pipe_total_events(const uft_pipe_ctx_t *ctx) {
    if (!ctx) return 0;
    return otdr11_get_stats(&ctx->pipeline).total_events;
}

/* ════════════════════════════════════════════════════════════════════
 * Utilities
 * ════════════════════════════════════════════════════════════════════ */

const char *uft_pipe_error_str(uft_pipe_error_t e) {
    switch (e) {
    case UFT_PIPE_OK:           return "OK";
    case UFT_PIPE_ERR_NULL:     return "NULL parameter";
    case UFT_PIPE_ERR_NOMEM:    return "Out of memory";
    case UFT_PIPE_ERR_SMALL:    return "Data too small";
    case UFT_PIPE_ERR_STATE:    return "Invalid state";
    case UFT_PIPE_ERR_INTERNAL: return "Internal error";
    default:                    return "Unknown error";
    }
}

const char *uft_pipe_version(void) { return V11_BRIDGE_VERSION; }
