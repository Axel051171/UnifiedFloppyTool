#include "otdr_event_core_v2.h"
#include "otdr_common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---------- helpers ---------- */

/* robust sigma from MAD on slice x[a..b] inclusive */
static float robust_sigma_mad_slice(const float *x, size_t a, size_t b, float mad_scale) {
    if (!x || b < a) return 0.0f;
    size_t n = b - a + 1;
    if (n == 0) return 0.0f;

    float *tmp = (float*)malloc(n * sizeof(float));
    if (!tmp) return 0.0f;

    for (size_t i = 0; i < n; i++) tmp[i] = x[a + i];
    qsort(tmp, n, sizeof(float), otdr_cmp_float_asc);
    float med = (n & 1u) ? tmp[n/2] : 0.5f * (tmp[n/2 - 1] + tmp[n/2]);

    for (size_t i = 0; i < n; i++) tmp[i] = fabsf(tmp[i] - med);
    qsort(tmp, n, sizeof(float), otdr_cmp_float_asc);
    float mad = (n & 1u) ? tmp[n/2] : 0.5f * (tmp[n/2 - 1] + tmp[n/2]);

    free(tmp);
    return mad_scale * mad;
}

static void sliding_rms_trailing(const float *x, size_t n, size_t win, float *out) {
    if (!x || !out || n == 0) return;
    if (win == 0) win = 1;

    double sumsq = 0.0;
    size_t a = 0;

    for (size_t i = 0; i < n; i++) {
        double v = (double)x[i];
        sumsq += v * v;

        if (i + 1 > win) {
            double u = (double)x[a++];
            sumsq -= u * u;
        }

        size_t cur = (i + 1 < win) ? (i + 1) : win;
        out[i] = (float)sqrt(sumsq / (double)cur);
    }
}

/* local sigma computed every stride samples using trailing sigma_window */
static int compute_local_sigma(const float *delta, size_t n, const otdr_config *cfg, float *sigma_out) {
    if (!delta || !cfg || !sigma_out || n == 0) return -1;

    size_t win = cfg->sigma_window ? cfg->sigma_window : 2048;
    size_t stride = cfg->sigma_stride ? cfg->sigma_stride : 256;
    float smin = (cfg->sigma_min > 0.0f) ? cfg->sigma_min : 1e-12f;

    size_t i = 0;
    while (i < n) {
        size_t end = i;
        size_t start = (end + 1 > win) ? (end + 1 - win) : 0;
        float sigma = robust_sigma_mad_slice(delta, start, end, cfg->mad_scale);
        if (sigma < smin) sigma = smin;

        size_t j_end = i + stride;
        if (j_end > n) j_end = n;
        for (size_t j = i; j < j_end; j++) sigma_out[j] = sigma;

        i += stride;
    }
    return 0;
}

/* ---------- public API ---------- */

otdr_config otdr_default_config(void) {
    otdr_config c;
    c.window = 1025;
    c.snr_floor_db = -60.0f;
    c.snr_ceil_db  =  60.0f;

    c.thr_reflect_snr_db = 12.0f;
    c.thr_atten_snr_db   = 10.0f;
    c.min_env_rms = 1e-4f;

    c.mad_scale = 1.4826f;

    c.local_sigma_enable = 1;
    c.sigma_window = 4096;
    c.sigma_stride = 256;
    c.sigma_min = 1e-12f;
    return c;
}

otdr_merge_config otdr_default_merge_config(void) {
    otdr_merge_config m;
    m.merge_gap_max = 64;
    m.min_reflection_len = 1;
    m.min_atten_len = 2;
    return m;
}

int otdr_extract_features(const float *amp, size_t n,
                          const otdr_config *cfg,
                          otdr_features_t *out_feat)
{
    if (!amp || !out_feat || !cfg || n == 0) return -1;

    float *delta = (float*)calloc(n, sizeof(float));
    float *env   = (float*)calloc(n, sizeof(float));
    float *sigma = (float*)calloc(n, sizeof(float));
    if (!delta || !env || !sigma) { free(delta); free(env); free(sigma); return -2; }

    delta[0] = 0.0f;
    for (size_t i = 1; i < n; i++) delta[i] = amp[i] - amp[i - 1];

    sliding_rms_trailing(amp, n, cfg->window, env);

    if (cfg->local_sigma_enable) {
        compute_local_sigma(delta, n, cfg, sigma);
    } else {
        float g = robust_sigma_mad_slice(delta, 0, n - 1, cfg->mad_scale);
        if (g < cfg->sigma_min) g = cfg->sigma_min;
        for (size_t i = 0; i < n; i++) sigma[i] = g;
    }

    for (size_t i = 0; i < n; i++) {
        float s = sigma[i];
        if (s < cfg->sigma_min) s = cfg->sigma_min;

        out_feat[i].amp = amp[i];
        out_feat[i].delta = delta[i];
        out_feat[i].env_rms = env[i];
        out_feat[i].noise_sigma = s;

        float d = fabsf(delta[i]);
        float snr = 20.0f * log10f(d / s);
        if (!isfinite(snr)) snr = cfg->snr_floor_db;
        out_feat[i].snr_db = otdr_clampf(snr, cfg->snr_floor_db, cfg->snr_ceil_db);
    }

    free(delta);
    free(env);
    free(sigma);
    return 0;
}

int otdr_classify_baseline(const otdr_features_t *feat, size_t n,
                           const otdr_config *cfg,
                           otdr_event_result_t *out_res)
{
    if (!feat || !out_res || !cfg || n == 0) return -1;

    for (size_t i = 0; i < n; i++) {
        otdr_event_t lab = OTDR_EVT_NONE;
        float conf = 0.0f;

        if (feat[i].env_rms >= cfg->min_env_rms) {
            float snr = feat[i].snr_db;
            float d = feat[i].delta;

            if (d > 0.0f && snr >= cfg->thr_reflect_snr_db) {
                lab = OTDR_EVT_REFLECTION;
                conf = otdr_clampf((snr - cfg->thr_reflect_snr_db) / 20.0f, 0.0f, 1.0f);
            } else if (d < 0.0f && snr >= cfg->thr_atten_snr_db) {
                lab = OTDR_EVT_ATTENUATION;
                conf = otdr_clampf((snr - cfg->thr_atten_snr_db) / 20.0f, 0.0f, 1.0f);
            }
        }

        out_res[i].label = lab;
        out_res[i].confidence = conf;
    }
    return 0;
}

int otdr_detect_events_baseline(const float *amp, size_t n,
                                const otdr_config *cfg,
                                otdr_features_t *out_feat,
                                otdr_event_result_t *out_res)
{
    if (!amp || !cfg || !out_res || n == 0) return -1;

    int rc;
    otdr_features_t *tmp = out_feat;
    if (!tmp) {
        tmp = (otdr_features_t*)calloc(n, sizeof(otdr_features_t));
        if (!tmp) return -2;
    }

    rc = otdr_extract_features(amp, n, cfg, tmp);
    if (rc != 0) { if (!out_feat) free(tmp); return rc; }

    rc = otdr_classify_baseline(tmp, n, cfg, out_res);

    if (!out_feat) free(tmp);
    return rc;
}

size_t otdr_rle_segments(const otdr_event_result_t *res, size_t n,
                         otdr_segment_t *seg_out, size_t max_seg)
{
    if (!res || !seg_out || n == 0 || max_seg == 0) return 0;

    size_t out_n = 0;
    size_t start = 0;
    otdr_event_t cur = res[0].label;
    double sumc = res[0].confidence;
    size_t cnt = 1;

    for (size_t i = 1; i < n; i++) {
        if (res[i].label == cur) {
            sumc += res[i].confidence;
            cnt++;
        } else {
            if (out_n < max_seg) {
                seg_out[out_n].start = start;
                seg_out[out_n].end = i - 1;
                seg_out[out_n].label = cur;
                seg_out[out_n].mean_conf = (float)(sumc / (double)cnt);
                seg_out[out_n].flags = OTDR_SEG_FLAG_NONE;
                out_n++;
            }
            start = i;
            cur = res[i].label;
            sumc = res[i].confidence;
            cnt = 1;
        }
    }

    if (out_n < max_seg) {
        seg_out[out_n].start = start;
        seg_out[out_n].end = n - 1;
        seg_out[out_n].label = cur;
        seg_out[out_n].mean_conf = (float)(sumc / (double)cnt);
        seg_out[out_n].flags = OTDR_SEG_FLAG_NONE;
        out_n++;
    }

    return out_n;
}

size_t otdr_rle_segments_merged(const otdr_event_result_t *res, size_t n,
                                const otdr_merge_config *mcfg,
                                otdr_segment_t *seg_out, size_t max_seg)
{
    if (!res || !seg_out || n == 0 || max_seg == 0) return 0;

    otdr_merge_config m = mcfg ? *mcfg : otdr_default_merge_config();

    /* First build raw segments */
    otdr_segment_t tmp[4096];
    size_t max_tmp = sizeof(tmp)/sizeof(tmp[0]);
    size_t ns = otdr_rle_segments(res, n, tmp, max_tmp);

    size_t out_n = 0;
    size_t i = 0;
    while (i < ns && out_n < max_seg) {
        otdr_segment_t s = tmp[i];

        /* candidate: REFLECTION run */
        if (s.label == OTDR_EVT_REFLECTION) {
            size_t refl_len = s.end - s.start + 1;
            if (refl_len >= m.min_reflection_len) {
                /* find next ATTENUATION possibly separated by NONE segments (small gap) */
                size_t j = i + 1;
                size_t gap = 0;
                size_t gap_end = s.end;

                while (j < ns) {
                    if (tmp[j].label == OTDR_EVT_NONE) {
                        gap += (tmp[j].end - tmp[j].start + 1);
                        gap_end = tmp[j].end;
                        if (gap > m.merge_gap_max) break;
                        j++;
                        continue;
                    }
                    break;
                }

                if (j < ns && tmp[j].label == OTDR_EVT_ATTENUATION) {
                    size_t att_len = tmp[j].end - tmp[j].start + 1;
                    if (att_len >= m.min_atten_len && gap <= m.merge_gap_max) {
                        /* merge into one segment */
                        otdr_segment_t merged;
                        merged.start = s.start;
                        merged.end = tmp[j].end;
                        merged.label = OTDR_EVT_REFLECT_LOSS;
                        /* confidence: average weighted by lengths over involved segments */
                        double sum = 0.0;
                        size_t cnt = 0;

                        /* reflection */
                        sum += (double)s.mean_conf * (double)refl_len;
                        cnt += refl_len;

                        /* any NONE gap contributes 0 confidence */
                        if (gap > 0) { cnt += gap; }

                        /* attenuation */
                        sum += (double)tmp[j].mean_conf * (double)att_len;
                        cnt += att_len;

                        merged.mean_conf = (float)((cnt > 0) ? (sum / (double)cnt) : 0.0);
                        merged.flags = OTDR_SEG_FLAG_MERGED;

                        seg_out[out_n++] = merged;

                        /* skip used segments: i..j */
                        i = j + 1;
                        continue;
                    }
                }
            }
        }

        /* default: pass through */
        seg_out[out_n++] = s;
        i++;
    }

    return out_n;
}
