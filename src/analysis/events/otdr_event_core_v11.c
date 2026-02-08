/*
 * otdr_event_core_v11.c — Pipeline API + Streaming
 * ==================================================
 * Ring buffer, chunked processing, stage dispatch, callbacks.
 */

#include "otdr_event_core_v11.h"
#include "otdr_event_core_v9.h"
#include "otdr_event_core_v8.h"
#include "otdr_event_core_v10.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ════════════════════════════════════════════════════════════════════
 * Ring buffer ops
 * ════════════════════════════════════════════════════════════════════ */

static int ring_init(otdr11_ring_t *r, size_t cap) {
    r->buf = (float *)calloc(cap, sizeof(float));
    if (!r->buf) return -1;
    r->capacity = cap;
    r->len = r->head = r->tail = 0;
    return 0;
}

static void ring_free(otdr11_ring_t *r) {
    free(r->buf);
    memset(r, 0, sizeof(*r));
}

static void ring_reset(otdr11_ring_t *r) {
    r->len = r->head = r->tail = 0;
}

static size_t ring_available(const otdr11_ring_t *r) {
    return r->len;
}

static size_t ring_push(otdr11_ring_t *r, const float *data, size_t n) {
    size_t space = r->capacity - r->len;
    if (n > space) n = space;
    for (size_t i = 0; i < n; i++) {
        r->buf[r->head] = data[i];
        r->head = (r->head + 1) % r->capacity;
    }
    r->len += n;
    return n;
}

/* Linearize n samples from tail into dst (zero-copy read) */
static void ring_peek(const otdr11_ring_t *r, float *dst, size_t n) {
    if (n > r->len) n = r->len;
    size_t pos = r->tail;
    for (size_t i = 0; i < n; i++) {
        dst[i] = r->buf[pos];
        pos = (pos + 1) % r->capacity;
    }
}

/* Advance tail by n (consume samples) */
static void ring_consume(otdr11_ring_t *r, size_t n) {
    if (n > r->len) n = r->len;
    r->tail = (r->tail + n) % r->capacity;
    r->len -= n;
}

/* ════════════════════════════════════════════════════════════════════
 * Defaults
 * ════════════════════════════════════════════════════════════════════ */

otdr11_config_t otdr11_default_config(void) {
    otdr11_config_t c;
    memset(&c, 0, sizeof(c));

    c.ring_capacity = 65536;
    c.chunk_size    = 8192;
    c.overlap       = 256;

    c.enable_integrity  = 1;
    c.enable_denoise    = 0;
    c.enable_detect     = 1;
    c.enable_confidence = 1;

    /* v9 */
    c.dropout_threshold = 1e-4f;
    c.dropout_min_run   = 3;
    c.clip_high         = 0.99f;
    c.clip_low          = -0.99f;
    c.stuck_max_delta   = 1e-6f;
    c.stuck_min_run     = 5;
    c.auto_repair       = 0;

    /* v8 */
    c.detect_snr_threshold = 12.0f;

    /* v10 */
    c.conf_w_agreement = 0.40f;
    c.conf_w_snr       = 0.35f;
    c.conf_w_integrity = 0.25f;

    c.on_chunk  = NULL;
    c.on_event  = NULL;
    c.user_data = NULL;

    return c;
}

/* ════════════════════════════════════════════════════════════════════
 * Init / Free / Reset
 * ════════════════════════════════════════════════════════════════════ */

int otdr11_init(otdr11_pipeline_t *p, const otdr11_config_t *cfg) {
    if (!p) return -1;
    memset(p, 0, sizeof(*p));
    p->cfg = cfg ? *cfg : otdr11_default_config();

    if (p->cfg.chunk_size < 32) p->cfg.chunk_size = 32;
    if (p->cfg.overlap >= p->cfg.chunk_size)
        p->cfg.overlap = p->cfg.chunk_size / 4;
    if (p->cfg.ring_capacity < p->cfg.chunk_size * 2)
        p->cfg.ring_capacity = p->cfg.chunk_size * 4;

    if (ring_init(&p->ring, p->cfg.ring_capacity) != 0)
        return -2;

    size_t cs = p->cfg.chunk_size;
    p->work_chunk  = (float *)calloc(cs, sizeof(float));
    p->work_flags  = (uint8_t *)calloc(cs, sizeof(uint8_t));
    p->work_conf   = (float *)calloc(cs, sizeof(float));
    p->work_events = (otdr11_event_t *)calloc(OTDR11_MAX_EVENTS_PER_CHUNK,
                                               sizeof(otdr11_event_t));

    if (!p->work_chunk || !p->work_flags || !p->work_conf || !p->work_events) {
        otdr11_free(p);
        return -2;
    }

    p->stats.state = OTDR11_STATE_IDLE;
    p->initialized = 1;
    return 0;
}

void otdr11_free(otdr11_pipeline_t *p) {
    if (!p) return;
    ring_free(&p->ring);
    free(p->work_chunk);
    free(p->work_flags);
    free(p->work_conf);
    free(p->work_events);
    memset(p, 0, sizeof(*p));
}

void otdr11_reset(otdr11_pipeline_t *p) {
    if (!p || !p->initialized) return;
    ring_reset(&p->ring);
    memset(&p->stats, 0, sizeof(p->stats));
    p->stats.state = OTDR11_STATE_IDLE;
}

/* ════════════════════════════════════════════════════════════════════
 * Process one chunk
 * ════════════════════════════════════════════════════════════════════ */

static void process_chunk(otdr11_pipeline_t *p, size_t chunk_len) {
    size_t cs = chunk_len;
    size_t abs_offset = p->stats.total_samples;

    /* Linearize from ring */
    ring_peek(&p->ring, p->work_chunk, cs);

    memset(p->work_flags, 0, cs);
    memset(p->work_conf, 0, cs * sizeof(float));

    size_t event_count = 0;
    float integrity_score = 1.0f;
    size_t flagged = 0;
    size_t integrity_regions = 0;

    /* ── Stage 1: Integrity (v9) ─────────────────────────────── */
    if (p->cfg.enable_integrity && cs >= 4) {
        otdr9_config_t c9 = otdr9_default_config();
        c9.dropout_threshold = p->cfg.dropout_threshold;
        c9.dropout_min_run   = p->cfg.dropout_min_run;
        c9.clip_high         = p->cfg.clip_high;
        c9.clip_low          = p->cfg.clip_low;
        c9.stuck_max_delta   = p->cfg.stuck_max_delta;
        c9.stuck_min_run     = p->cfg.stuck_min_run;
        c9.auto_repair       = p->cfg.auto_repair;

        otdr9_region_t regs[256];
        otdr9_summary_t summ;
        int nr = otdr9_scan(p->work_chunk, cs, &c9,
                            p->work_flags, regs, 256, &summ);
        if (nr >= 0) {
            integrity_regions = (size_t)nr;
            integrity_score = summ.integrity_score;
            flagged = summ.flagged_samples;
        }

        if (p->cfg.auto_repair)
            otdr9_repair(p->work_chunk, cs, p->work_flags);
    }

    /* ── Stage 2: Denoise (placeholder) ──────────────────────── */
    /* Reserved for φ-OTDR integration */

    /* ── Stage 3: Detect (v8 simplified) ─────────────────────── */
    if (p->cfg.enable_detect && cs >= 4) {
        /*
         * Simplified v8-compatible detection:
         * Compute local delta, find regions exceeding threshold.
         * Full v8 multi-scale is heavyweight; here we do a fast
         * single-pass scan suitable for streaming.
         */
        float thr = p->cfg.detect_snr_threshold;

        /* Compute local delta magnitudes */
        float *dmag = (float *)calloc(cs, sizeof(float));
        if (dmag) {
            for (size_t i = 1; i < cs; i++)
                dmag[i] = fabsf(p->work_chunk[i] - p->work_chunk[i - 1]);

            /* Quick mean-based noise estimate */
            float sigma = 1e-10f;
            {
                double sum = 0;
                for (size_t i = 1; i < cs; i++) sum += (double)dmag[i];
                sigma = (float)(sum / (double)(cs - 1));
                if (sigma < 1e-15f) sigma = 1e-15f;
            }

            /* Detect events above threshold * sigma */
            float abs_thr = thr * sigma;
            size_t run_start = 0;
            int in_run = 0;

            for (size_t i = 0; i <= cs; i++) {
                int above = (i < cs) && (dmag[i] > abs_thr) &&
                            !(p->work_flags[i] & 0x40);  /* not excluded */

                if (above && !in_run) { run_start = i; in_run = 1; }
                else if (!above && in_run) {
                    if (event_count < OTDR11_MAX_EVENTS_PER_CHUNK) {
                        otdr11_event_t *e = &p->work_events[event_count];
                        e->type       = 1;  /* generic event */
                        e->abs_start  = (uint32_t)(abs_offset + run_start);
                        e->abs_end    = (uint32_t)(abs_offset + i - 1);
                        e->severity   = dmag[run_start] / abs_thr;
                        if (e->severity > 1.0f) e->severity = 1.0f;
                        e->flags      = p->work_flags[run_start];
                        e->confidence = 0.0f;  /* filled by v10 below */
                        event_count++;
                    }
                    in_run = 0;
                }
            }
            free(dmag);
        }
    }

    /* ── Stage 4: Confidence (v10) ───────────────────────────── */
    float mean_conf = 0.5f, min_conf = 0.5f;
    if (p->cfg.enable_confidence && cs >= 2) {
        otdr10_config_t c10 = otdr10_default_config();
        c10.w_agreement = p->cfg.conf_w_agreement;
        c10.w_snr       = p->cfg.conf_w_snr;
        c10.w_integrity = p->cfg.conf_w_integrity;

        otdr10_sample_t *samp = (otdr10_sample_t *)calloc(cs, sizeof(otdr10_sample_t));
        if (samp) {
            /* No agreement array in streaming mode → use default */
            otdr10_compute(NULL, NULL, p->work_flags, cs, &c10, samp);

            double csum = 0;
            min_conf = 2.0f;
            for (size_t i = 0; i < cs; i++) {
                p->work_conf[i] = samp[i].confidence;
                csum += (double)samp[i].confidence;
                if (samp[i].confidence < min_conf)
                    min_conf = samp[i].confidence;
            }
            mean_conf = (float)(csum / (double)cs);

            /* Back-fill event confidence */
            for (size_t e = 0; e < event_count; e++) {
                uint32_t es = p->work_events[e].abs_start - (uint32_t)abs_offset;
                uint32_t ee = p->work_events[e].abs_end - (uint32_t)abs_offset;
                if (es < cs && ee < cs) {
                    double esum = 0;
                    for (uint32_t j = es; j <= ee; j++)
                        esum += (double)samp[j].confidence;
                    p->work_events[e].confidence =
                        (float)(esum / (double)(ee - es + 1));
                }
            }
            free(samp);
        }
    }

    /* ── Build chunk result ──────────────────────────────────── */
    otdr11_chunk_result_t result;
    memset(&result, 0, sizeof(result));
    result.chunk_id          = p->stats.chunks_processed;
    result.chunk_offset      = abs_offset;
    result.chunk_len         = cs;
    result.integrity_flags   = p->work_flags;
    result.integrity_regions = integrity_regions;
    result.flagged_samples   = flagged;
    result.integrity_score   = integrity_score;
    result.events            = p->work_events;
    result.event_count       = event_count;
    result.confidence        = p->work_conf;
    result.mean_confidence   = mean_conf;
    result.min_confidence    = min_conf;

    /* ── Fire callbacks ──────────────────────────────────────── */
    if (p->cfg.on_event) {
        for (size_t e = 0; e < event_count; e++)
            p->cfg.on_event(&p->work_events[e], p->cfg.user_data);
    }
    if (p->cfg.on_chunk)
        p->cfg.on_chunk(&result, p->cfg.user_data);

    /* ── Update stats ────────────────────────────────────────── */
    p->stats.total_samples += cs;
    p->stats.chunks_processed++;
    p->stats.total_events += event_count;
    p->stats.total_flagged += flagged;

    /* Running averages */
    float n_chunks = (float)p->stats.chunks_processed;
    p->stats.mean_integrity = p->stats.mean_integrity
        + (integrity_score - p->stats.mean_integrity) / n_chunks;
    p->stats.mean_confidence = p->stats.mean_confidence
        + (mean_conf - p->stats.mean_confidence) / n_chunks;
    if (p->stats.chunks_processed == 1 || min_conf < p->stats.min_confidence)
        p->stats.min_confidence = min_conf;

    /* Consume processed samples (minus overlap for next chunk) */
    size_t advance = cs;
    if (p->cfg.overlap < cs)
        advance = cs - p->cfg.overlap;
    ring_consume(&p->ring, advance);
}

/* ════════════════════════════════════════════════════════════════════
 * Push / Flush
 * ════════════════════════════════════════════════════════════════════ */

int otdr11_push(otdr11_pipeline_t *p, const float *samples, size_t n) {
    if (!p || !p->initialized || !samples) return -1;
    if (n == 0) return 0;

    p->stats.state = OTDR11_STATE_RUNNING;

    size_t pushed = ring_push(&p->ring, samples, n);
    (void)pushed;  /* best effort */

    int chunks = 0;
    while (ring_available(&p->ring) >= p->cfg.chunk_size) {
        process_chunk(p, p->cfg.chunk_size);
        chunks++;
    }
    return chunks;
}

int otdr11_flush(otdr11_pipeline_t *p) {
    if (!p || !p->initialized) return -1;

    p->stats.state = OTDR11_STATE_FLUSHING;

    int chunks = 0;
    size_t avail = ring_available(&p->ring);
    if (avail >= 4) {  /* minimum viable chunk */
        process_chunk(p, avail);
        ring_consume(&p->ring, avail);  /* consume all remaining */
        chunks = 1;
    }

    p->stats.state = OTDR11_STATE_DONE;
    return chunks;
}

/* ════════════════════════════════════════════════════════════════════
 * Stats / Strings
 * ════════════════════════════════════════════════════════════════════ */

otdr11_stats_t otdr11_get_stats(const otdr11_pipeline_t *p) {
    otdr11_stats_t empty;
    memset(&empty, 0, sizeof(empty));
    return p ? p->stats : empty;
}

const char *otdr11_stage_str(otdr11_stage_t s) {
    switch (s) {
    case OTDR11_STAGE_INTEGRITY:  return "INTEGRITY";
    case OTDR11_STAGE_DENOISE:    return "DENOISE";
    case OTDR11_STAGE_DETECT:     return "DETECT";
    case OTDR11_STAGE_CONFIDENCE: return "CONFIDENCE";
    default:                      return "UNKNOWN";
    }
}

const char *otdr11_state_str(otdr11_state_t s) {
    switch (s) {
    case OTDR11_STATE_IDLE:     return "IDLE";
    case OTDR11_STATE_RUNNING:  return "RUNNING";
    case OTDR11_STATE_FLUSHING: return "FLUSHING";
    case OTDR11_STATE_DONE:     return "DONE";
    default:                    return "UNKNOWN";
    }
}
