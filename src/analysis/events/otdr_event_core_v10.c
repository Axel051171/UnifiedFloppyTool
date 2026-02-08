/*
 * otdr_event_core_v10.c — Multi-Pass Consensus: Confidence Map
 * ==============================================================
 * Fuses agreement (v7) + SNR (v8) + integrity (v9) → per-sample confidence.
 */

#include "otdr_event_core_v10.h"
#include "otdr_common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ════════════════════════════════════════════════════════════════════
 * Helpers
 * ════════════════════════════════════════════════════════════════════ */

static int cmp_seg_conf_desc(const void *a, const void *b) {
    const otdr10_segment_t *sa = (const otdr10_segment_t *)a;
    const otdr10_segment_t *sb = (const otdr10_segment_t *)b;
    if (sa->mean_confidence > sb->mean_confidence) return -1;
    if (sa->mean_confidence < sb->mean_confidence) return  1;
    return 0;
}

/* Integrity flag bits that indicate bad data (from v9) */
#define BAD_MASK  0x1Fu  /* bits 0-4: dropout, clip_hi, clip_lo, stuck, deadzone */
#define REPAIRED  0x20u  /* bit 5 */

/* ════════════════════════════════════════════════════════════════════
 * Defaults
 * ════════════════════════════════════════════════════════════════════ */

otdr10_config_t otdr10_default_config(void) {
    otdr10_config_t c;
    memset(&c, 0, sizeof(c));

    c.w_agreement  = 0.40f;
    c.w_snr        = 0.35f;
    c.w_integrity  = 0.25f;

    c.snr_floor_db = -10.0f;
    c.snr_ceil_db  =  40.0f;

    c.integ_clean    = 1.0f;
    c.integ_flagged  = 0.0f;
    c.integ_repaired = 0.30f;

    c.min_segment_len  = 16;
    c.default_agreement = 0.5f;
    c.default_snr_db    = 10.0f;

    return c;
}

/* ════════════════════════════════════════════════════════════════════
 * Compute confidence map
 * ════════════════════════════════════════════════════════════════════ */

int otdr10_compute(const float *agree,
                   const float *snr_db,
                   const uint8_t *flags,
                   size_t n,
                   const otdr10_config_t *cfg,
                   otdr10_sample_t *out) {
    if (!out || n == 0) return -1;

    otdr10_config_t c = cfg ? *cfg : otdr10_default_config();

    /* Normalize weights */
    float wsum = c.w_agreement + c.w_snr + c.w_integrity;
    if (wsum < 1e-6f) wsum = 1.0f;
    float wa = c.w_agreement / wsum;
    float ws = c.w_snr / wsum;
    float wi = c.w_integrity / wsum;

    float snr_range = c.snr_ceil_db - c.snr_floor_db;
    if (snr_range < 1.0f) snr_range = 1.0f;

    for (size_t i = 0; i < n; i++) {
        /* Agreement component */
        float a_raw = agree ? otdr_clampf(agree[i], 0.0f, 1.0f) : c.default_agreement;

        /* SNR component: normalize to 0..1 */
        float s_raw = snr_db ? snr_db[i] : c.default_snr_db;
        float s_norm = otdr_clampf((s_raw - c.snr_floor_db) / snr_range, 0.0f, 1.0f);

        /* Integrity component */
        float integ;
        if (!flags) {
            integ = c.integ_clean;
        } else if (flags[i] & BAD_MASK) {
            integ = (flags[i] & REPAIRED) ? c.integ_repaired : c.integ_flagged;
        } else {
            integ = c.integ_clean;
        }

        out[i].agree_comp = wa * a_raw;
        out[i].snr_comp   = ws * s_norm;
        out[i].integ_comp = wi * integ;
        out[i].confidence = otdr_clampf(
            out[i].agree_comp + out[i].snr_comp + out[i].integ_comp,
            0.0f, 1.0f);
    }

    return 0;
}

/* ════════════════════════════════════════════════════════════════════
 * Segment + Rank
 * ════════════════════════════════════════════════════════════════════ */

size_t otdr10_segment_rank(const otdr10_sample_t *samples, size_t n,
                           const otdr10_config_t *cfg,
                           otdr10_segment_t *seg_out, size_t max_seg) {
    if (!samples || !seg_out || n == 0 || max_seg == 0) return 0;

    otdr10_config_t c = cfg ? *cfg : otdr10_default_config();
    size_t min_len = c.min_segment_len;
    if (min_len < 1) min_len = 1;

    /* Quantize confidence into bands: HIGH(>=0.7), MID(0.3-0.7), LOW(<0.3) */
    size_t nseg = 0;
    size_t seg_start = 0;
    int cur_band = (samples[0].confidence >= 0.7f) ? 2 :
                   (samples[0].confidence >= 0.3f) ? 1 : 0;

    for (size_t i = 1; i <= n; i++) {
        int band = (i < n) ?
            ((samples[i].confidence >= 0.7f) ? 2 :
             (samples[i].confidence >= 0.3f) ? 1 : 0) : -1;

        if (band != cur_band) {
            size_t len = i - seg_start;
            if (len >= min_len && nseg < max_seg) {
                otdr10_segment_t *s = &seg_out[nseg];
                s->start = seg_start;
                s->end   = i - 1;

                /* Compute segment metrics */
                double sum_c = 0, sum_a = 0, sum_s = 0, sum_i = 0;
                float min_c = 2.0f;
                size_t flagged = 0;

                for (size_t j = seg_start; j < i; j++) {
                    sum_c += (double)samples[j].confidence;
                    sum_a += (double)samples[j].agree_comp;
                    sum_s += (double)samples[j].snr_comp;
                    sum_i += (double)samples[j].integ_comp;
                    if (samples[j].confidence < min_c)
                        min_c = samples[j].confidence;
                    if (samples[j].integ_comp < 0.2f)
                        flagged++;
                }

                s->mean_confidence = (float)(sum_c / (double)len);
                s->min_confidence  = min_c;
                s->mean_agreement  = (float)(sum_a / (double)len);
                s->mean_snr_norm   = (float)(sum_s / (double)len);
                s->mean_integrity  = (float)(sum_i / (double)len);
                s->flagged_count   = flagged;
                s->rank            = 0;  /* assigned after sort */

                nseg++;
            }
            seg_start = i;
            cur_band = band;
        }
    }

    /* Sort by confidence descending and assign ranks */
    qsort(seg_out, nseg, sizeof(otdr10_segment_t), cmp_seg_conf_desc);
    for (size_t i = 0; i < nseg; i++)
        seg_out[i].rank = i;

    return nseg;
}

/* ════════════════════════════════════════════════════════════════════
 * Summary
 * ════════════════════════════════════════════════════════════════════ */

int otdr10_summarize(const otdr10_sample_t *samples, size_t n,
                     const otdr10_segment_t *segs, size_t nseg,
                     otdr10_summary_t *out) {
    if (!samples || !out || n == 0) return -1;

    memset(out, 0, sizeof(*out));
    out->n = n;
    out->num_segments = nseg;

    double sum_c = 0, sum_a = 0, sum_s = 0, sum_i = 0;
    float min_c = 2.0f, max_c = -1.0f;

    for (size_t i = 0; i < n; i++) {
        float c = samples[i].confidence;
        sum_c += (double)c;
        sum_a += (double)samples[i].agree_comp;
        sum_s += (double)samples[i].snr_comp;
        sum_i += (double)samples[i].integ_comp;
        if (c < min_c) min_c = c;
        if (c > max_c) max_c = c;

        if (c >= 0.8f)      out->high_conf_count++;
        else if (c >= 0.4f) out->mid_conf_count++;
        else                out->low_conf_count++;
    }

    out->mean_confidence = (float)(sum_c / (double)n);
    out->min_confidence  = min_c;
    out->max_confidence  = max_c;
    out->mean_agreement  = (float)(sum_a / (double)n);
    out->mean_snr_norm   = (float)(sum_s / (double)n);
    out->mean_integrity  = (float)(sum_i / (double)n);

    out->high_conf_frac = (float)out->high_conf_count / (float)n;
    out->low_conf_frac  = (float)out->low_conf_count / (float)n;

    /* Median */
    float *tmp = (float *)malloc(n * sizeof(float));
    if (tmp) {
        for (size_t i = 0; i < n; i++) tmp[i] = samples[i].confidence;
        qsort(tmp, n, sizeof(float), otdr_cmp_float_asc);
        out->median_confidence = (n & 1u)
            ? tmp[n / 2]
            : 0.5f * (tmp[n / 2 - 1] + tmp[n / 2]);
        free(tmp);
    }

    /* Overall quality: weighted combo of mean confidence and high-conf fraction */
    out->overall_quality = 0.6f * out->mean_confidence + 0.4f * out->high_conf_frac;
    if (out->overall_quality > 1.0f) out->overall_quality = 1.0f;

    return 0;
}
