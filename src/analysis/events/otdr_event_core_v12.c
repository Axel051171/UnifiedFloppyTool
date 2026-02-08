/*
 * otdr_event_core_v12.c — Export / Integration / Golden Vectors
 * ==============================================================
 */

#include "otdr_event_core_v12.h"
#include "otdr_event_core_v9.h"
#include "otdr_event_core_v10.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ════════════════════════════════════════════════════════════════════
 * Version registry
 * ════════════════════════════════════════════════════════════════════ */

static const otdr12_module_ver_t g_modules[] = {
    {"otdr_event_core_v2",  "2.0.0",  2, 0},
    {"phi_otdr_denoise",    "1.0.0",  1, 0},
    {"otdr_align_fuse_v7",  "7.0.0",  7, 0},
    {"otdr_event_core_v8",  "8.0.0",  8, 0},
    {"otdr_event_core_v9",  "9.0.0",  9, 0},
    {"otdr_event_core_v10", "10.0.0", 10, 0},
    {"otdr_event_core_v11", "11.0.0", 11, 0},
    {"otdr_event_core_v12", "12.0.0", 12, 0},
};
#define N_MODULES (sizeof(g_modules)/sizeof(g_modules[0]))

static const char g_full_version[] = "UFT-NX Analysis Pipeline v12.0.0";

size_t otdr12_module_count(void) { return N_MODULES; }

const otdr12_module_ver_t *otdr12_module_version(size_t idx) {
    return (idx < N_MODULES) ? &g_modules[idx] : NULL;
}

const char *otdr12_full_version(void) { return g_full_version; }

/* ════════════════════════════════════════════════════════════════════
 * Golden vectors — built-in reference traces
 * ════════════════════════════════════════════════════════════════════ */

static const otdr12_golden_info_t g_goldens[] = {
    {
        "clean_fiber",
        "Clean signal: no anomalies, high confidence",
        OTDR12_GOLDEN_SIZE, 0.90f, 0.60f, 0, 10, 0, 0.15f
    },
    {
        "connector_dropout",
        "Signal with 50-sample dropout at position 2000",
        OTDR12_GOLDEN_SIZE, 0.80f, 0.55f, 1, 30, 50, 0.20f
    },
    {
        "multi_fault",
        "Dropout + clipping + stuck-at fault",
        OTDR12_GOLDEN_SIZE, 0.70f, 0.50f, 3, 50, 80, 0.25f
    },
    {
        "noisy_degraded",
        "High noise, gradual degradation",
        OTDR12_GOLDEN_SIZE, 1.00f, 0.59f, 0, 40, 0, 0.15f
    },
    {
        "saturation_burst",
        "Brief clipping at rail followed by recovery",
        OTDR12_GOLDEN_SIZE, 0.85f, 0.55f, 1, 20, 10, 0.20f
    },
};
#define N_GOLDENS (sizeof(g_goldens)/sizeof(g_goldens[0]))

size_t otdr12_golden_count(void) { return N_GOLDENS; }

const otdr12_golden_info_t *otdr12_golden_info(size_t idx) {
    return (idx < N_GOLDENS) ? &g_goldens[idx] : NULL;
}

/* Deterministic PRNG for reproducible golden vectors */
static uint32_t golden_rng(uint32_t *state) {
    *state = *state * 1103515245u + 12345u;
    return (*state >> 16) & 0x7FFF;
}

static float golden_randf(uint32_t *state) {
    return (float)golden_rng(state) / 32767.0f;
}

int otdr12_golden_generate(size_t idx, float *out, size_t n) {
    if (idx >= N_GOLDENS || !out || n < 16) return -1;

    uint32_t rng = (uint32_t)(idx * 7919u + 42u);
    size_t gn = (n < OTDR12_GOLDEN_SIZE) ? n : OTDR12_GOLDEN_SIZE;

    /* Base: clean signal with small noise */
    for (size_t i = 0; i < gn; i++)
        out[i] = 0.5f + (golden_randf(&rng) - 0.5f) * 0.01f;

    switch (idx) {
    case 0: /* clean — already done */
        break;

    case 1: /* connector dropout at 2000 */
        for (size_t i = 2000; i < 2050 && i < gn; i++)
            out[i] = 0.0f;
        break;

    case 2: /* multi fault */
        for (size_t i = 1000; i < 1030 && i < gn; i++) out[i] = 0.0f;       /* dropout */
        for (size_t i = 2500; i < 2520 && i < gn; i++) out[i] = 0.995f;     /* clipping */
        for (size_t i = 3500; i < 3540 && i < gn; i++) out[i] = 0.333f;     /* stuck */
        break;

    case 3: /* noisy degraded */
        for (size_t i = 0; i < gn; i++) {
            float t = (float)i / (float)gn;
            out[i] = 0.5f * (1.0f - 0.5f * t) + (golden_randf(&rng) - 0.5f) * 0.1f;
        }
        break;

    case 4: /* saturation burst */
        for (size_t i = 1500; i < 1515 && i < gn; i++) out[i] = 0.995f;
        break;

    default:
        return -1;
    }

    /* Zero-fill remainder */
    for (size_t i = gn; i < n; i++) out[i] = 0.5f;

    return 0;
}

int otdr12_golden_validate(size_t idx, const otdr12_result_t *result) {
    if (idx >= N_GOLDENS || !result) return -1;

    const otdr12_golden_info_t *g = &g_goldens[idx];
    float tol = g->tolerance;

    /* Integrity score within tolerance */
    if (fabsf(result->integrity_score - g->expected_integrity) > tol)
        return 1;

    /* Confidence within tolerance */
    if (fabsf(result->mean_confidence - g->expected_confidence) > tol)
        return 2;

    /* Event count within range */
    if (result->n_events < g->expected_min_events)
        return 3;
    if (result->n_events > g->expected_max_events)
        return 4;

    /* Flagged samples at least expected */
    if (result->flagged_samples < g->expected_min_flagged)
        return 5;

    return 0;  /* PASS */
}

/* ════════════════════════════════════════════════════════════════════
 * End-to-end analysis
 * ════════════════════════════════════════════════════════════════════ */

int otdr12_analyze(const float *signal, size_t n, otdr12_result_t *result) {
    if (!signal || !result || n < 16) return -1;

    memset(result, 0, sizeof(*result));
    result->n_samples = n;

    /* Allocate per-sample arrays */
    result->flags      = (uint8_t *)calloc(n, sizeof(uint8_t));
    result->confidence = (float *)calloc(n, sizeof(float));
    if (!result->flags || !result->confidence) {
        otdr12_free_result(result);
        return -2;
    }

    /* ── Stage 1: Integrity (v9) ──────────────────────────────── */
    otdr9_config_t c9 = otdr9_default_config();
    otdr9_region_t *regs9 = (otdr9_region_t *)calloc(1024, sizeof(otdr9_region_t));
    otdr9_summary_t summ9;

    if (regs9) {
        int nr = otdr9_scan(signal, n, &c9, result->flags, regs9, 1024, &summ9);
        if (nr >= 0) {
            result->integrity_score = summ9.integrity_score;
            result->flagged_samples = summ9.flagged_samples;
            result->dropout_count   = summ9.dropout_count;
            result->saturated_count = summ9.saturated_count;
            result->stuck_count     = summ9.stuck_count;
            result->deadzone_count  = summ9.deadzone_count;

            /* Convert v9 regions to v12 events */
            result->n_events = (size_t)nr;
            result->events = (otdr12_event_t *)calloc(result->n_events,
                                                       sizeof(otdr12_event_t));
            if (result->events) {
                for (size_t i = 0; i < result->n_events; i++) {
                    result->events[i].type       = (uint8_t)regs9[i].type;
                    result->events[i].start      = (uint32_t)regs9[i].start;
                    result->events[i].end        = (uint32_t)regs9[i].end;
                    result->events[i].severity   = regs9[i].severity;
                    result->events[i].flags      = result->flags[regs9[i].start];
                    result->events[i].confidence = 0.0f;  /* filled below */
                }
            }
        }
        free(regs9);
    }

    /* ── Stage 2: Confidence (v10) ────────────────────────────── */
    otdr10_config_t c10 = otdr10_default_config();
    otdr10_sample_t *samp10 = (otdr10_sample_t *)calloc(n, sizeof(otdr10_sample_t));

    if (samp10) {
        otdr10_compute(NULL, NULL, result->flags, n, &c10, samp10);

        double csum = 0;
        float cmin = 2.0f, cmax = -1.0f;
        for (size_t i = 0; i < n; i++) {
            result->confidence[i] = samp10[i].confidence;
            csum += (double)samp10[i].confidence;
            if (samp10[i].confidence < cmin) cmin = samp10[i].confidence;
            if (samp10[i].confidence > cmax) cmax = samp10[i].confidence;

            if (samp10[i].confidence >= 0.8f) result->high_conf_count++;
            else if (samp10[i].confidence >= 0.4f) result->mid_conf_count++;
            else result->low_conf_count++;
        }
        result->mean_confidence = (float)(csum / (double)n);
        result->min_confidence  = cmin;
        result->max_confidence  = cmax;

        /* Median */
        float *tmp = (float *)malloc(n * sizeof(float));
        if (tmp) {
            for (size_t i = 0; i < n; i++) tmp[i] = samp10[i].confidence;
            /* Simple selection for median: sort */
            for (size_t i = 0; i < n - 1; i++)
                for (size_t j = i + 1; j < n && j < i + 50; j++)
                    if (tmp[j] < tmp[i]) { float t = tmp[i]; tmp[i] = tmp[j]; tmp[j] = t; }
            /* Use qsort for accuracy */
            /* (We'll use a simple approach: just take middle element after partial sort) */
            /* Actually let's just sort properly */
            free(tmp);
            tmp = (float *)malloc(n * sizeof(float));
            if (tmp) {
                memcpy(tmp, result->confidence, n * sizeof(float));
                /* Insertion sort for golden-vector sizes (4096) — fast enough */
                for (size_t i = 1; i < n; i++) {
                    float key = tmp[i];
                    size_t j = i;
                    while (j > 0 && tmp[j - 1] > key) {
                        tmp[j] = tmp[j - 1];
                        j--;
                    }
                    tmp[j] = key;
                }
                result->median_confidence = (n & 1u)
                    ? tmp[n / 2]
                    : 0.5f * (tmp[n / 2 - 1] + tmp[n / 2]);
                free(tmp);
            }
        }

        /* Back-fill event confidence */
        if (result->events) {
            for (size_t e = 0; e < result->n_events; e++) {
                uint32_t es = result->events[e].start;
                uint32_t ee = result->events[e].end;
                if (es < n && ee < n) {
                    double esum = 0;
                    for (uint32_t j = es; j <= ee; j++)
                        esum += (double)samp10[j].confidence;
                    result->events[e].confidence =
                        (float)(esum / (double)(ee - es + 1));
                }
            }
        }

        /* Segments */
        otdr10_segment_t *raw_seg = (otdr10_segment_t *)calloc(512, sizeof(otdr10_segment_t));
        if (raw_seg) {
            size_t nseg = otdr10_segment_rank(samp10, n, &c10, raw_seg, 512);
            result->n_segments = nseg;
            result->segments = (otdr12_segment_t *)calloc(nseg, sizeof(otdr12_segment_t));
            if (result->segments) {
                for (size_t i = 0; i < nseg; i++) {
                    result->segments[i].start           = raw_seg[i].start;
                    result->segments[i].end             = raw_seg[i].end;
                    result->segments[i].mean_confidence = raw_seg[i].mean_confidence;
                    result->segments[i].rank            = raw_seg[i].rank;
                }
            }
            free(raw_seg);
        }
        free(samp10);
    }

    /* Overall quality */
    result->overall_quality = 0.4f * result->integrity_score
                            + 0.4f * result->mean_confidence
                            + 0.2f * (1.0f - (float)result->n_events * 0.02f);
    if (result->overall_quality < 0.0f) result->overall_quality = 0.0f;
    if (result->overall_quality > 1.0f) result->overall_quality = 1.0f;

    return 0;
}

void otdr12_free_result(otdr12_result_t *r) {
    if (!r) return;
    free(r->flags);
    free(r->confidence);
    free(r->events);
    free(r->segments);
    memset(r, 0, sizeof(*r));
}

/* ════════════════════════════════════════════════════════════════════
 * Export
 * ════════════════════════════════════════════════════════════════════ */

static int export_json(const otdr12_result_t *r, char *buf, size_t buflen) {
    /* Estimate size */
    size_t est = 512 + r->n_events * 128 + r->n_segments * 80;

    if (!buf) return (int)est;
    if (buflen < est) return -3;

    int pos = 0;
    pos += snprintf(buf + pos, buflen - (size_t)pos,
        "{\n"
        "  \"version\": \"%s\",\n"
        "  \"samples\": %zu,\n"
        "  \"integrity\": {\n"
        "    \"score\": %.4f,\n"
        "    \"flagged\": %zu,\n"
        "    \"dropouts\": %zu,\n"
        "    \"saturated\": %zu,\n"
        "    \"stuck\": %zu,\n"
        "    \"deadzones\": %zu\n"
        "  },\n"
        "  \"confidence\": {\n"
        "    \"mean\": %.4f,\n"
        "    \"median\": %.4f,\n"
        "    \"min\": %.4f,\n"
        "    \"max\": %.4f,\n"
        "    \"high_count\": %zu,\n"
        "    \"mid_count\": %zu,\n"
        "    \"low_count\": %zu\n"
        "  },\n"
        "  \"overall_quality\": %.4f,\n",
        g_full_version, r->n_samples,
        r->integrity_score, r->flagged_samples,
        r->dropout_count, r->saturated_count, r->stuck_count, r->deadzone_count,
        r->mean_confidence, r->median_confidence,
        r->min_confidence, r->max_confidence,
        r->high_conf_count, r->mid_conf_count, r->low_conf_count,
        r->overall_quality);

    /* Events */
    pos += snprintf(buf + pos, buflen - (size_t)pos, "  \"events\": [\n");
    for (size_t i = 0; i < r->n_events; i++) {
        const otdr12_event_t *e = &r->events[i];
        pos += snprintf(buf + pos, buflen - (size_t)pos,
            "    {\"type\":%u,\"start\":%u,\"end\":%u,\"confidence\":%.3f,\"severity\":%.3f}%s\n",
            e->type, e->start, e->end, e->confidence, e->severity,
            (i + 1 < r->n_events) ? "," : "");
    }
    pos += snprintf(buf + pos, buflen - (size_t)pos, "  ],\n");

    /* Segments */
    pos += snprintf(buf + pos, buflen - (size_t)pos, "  \"segments\": [\n");
    for (size_t i = 0; i < r->n_segments; i++) {
        const otdr12_segment_t *s = &r->segments[i];
        pos += snprintf(buf + pos, buflen - (size_t)pos,
            "    {\"start\":%zu,\"end\":%zu,\"confidence\":%.3f,\"rank\":%zu}%s\n",
            s->start, s->end, s->mean_confidence, s->rank,
            (i + 1 < r->n_segments) ? "," : "");
    }
    pos += snprintf(buf + pos, buflen - (size_t)pos, "  ]\n}\n");

    return pos;
}

static int export_csv(const otdr12_result_t *r, char *buf, size_t buflen) {
    size_t est = 128 + r->n_events * 80;
    if (!buf) return (int)est;
    if (buflen < est) return -3;

    int pos = 0;
    pos += snprintf(buf + pos, buflen - (size_t)pos,
        "# UFT-NX Analysis Export (CSV)\n"
        "# samples=%zu integrity=%.4f confidence=%.4f quality=%.4f\n"
        "type,start,end,confidence,severity,flags\n",
        r->n_samples, r->integrity_score, r->mean_confidence, r->overall_quality);

    for (size_t i = 0; i < r->n_events; i++) {
        const otdr12_event_t *e = &r->events[i];
        pos += snprintf(buf + pos, buflen - (size_t)pos,
            "%u,%u,%u,%.4f,%.4f,%u\n",
            e->type, e->start, e->end, e->confidence, e->severity, e->flags);
    }
    return pos;
}

static int export_binary(const otdr12_result_t *r, char *buf, size_t buflen) {
    size_t hdr_size = sizeof(otdr12_bin_header_t);
    size_t evt_size = r->n_events * sizeof(otdr12_event_t);
    size_t total = hdr_size + evt_size;

    if (!buf) return (int)total;
    if (buflen < total) return -3;

    otdr12_bin_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    memcpy(hdr.magic, "UFTx", 4);
    hdr.version         = 12;
    hdr.flags           = 0x07u;  /* all present */
    hdr.n_samples       = (uint32_t)r->n_samples;
    hdr.n_events        = (uint32_t)r->n_events;
    hdr.n_segments      = (uint32_t)r->n_segments;
    hdr.mean_confidence = r->mean_confidence;
    hdr.integrity_score = r->integrity_score;
    hdr.overall_quality = r->overall_quality;

    memcpy(buf, &hdr, hdr_size);
    if (r->events && r->n_events > 0)
        memcpy(buf + hdr_size, r->events, evt_size);

    return (int)total;
}

int otdr12_export(const otdr12_result_t *result, otdr12_format_t fmt,
                  char *buf, size_t buflen) {
    if (!result) return -1;

    switch (fmt) {
    case OTDR12_FMT_JSON:   return export_json(result, buf, buflen);
    case OTDR12_FMT_CSV:    return export_csv(result, buf, buflen);
    case OTDR12_FMT_BINARY: return export_binary(result, buf, buflen);
    default:                return -1;
    }
}
