/*
 * otdr_event_core_v8.c — Multi-Scale Event Model
 * =================================================
 * Implements: multi-scale features, polarity patterns,
 * extended classification, smart RL merge, pass/fail.
 */

#include "otdr_event_core_v8.h"
#include "otdr_common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ════════════════════════════════════════════════════════════════════
 * Helpers
 * ════════════════════════════════════════════════════════════════════ */

/* Robust sigma via MAD on slice */
static float robust_sigma_slice(const float *x, size_t a, size_t b, float mad_scale) {
    if (!x || b < a) return 0.0f;
    size_t n = b - a + 1;
    float *tmp = (float *)malloc(n * sizeof(float));
    if (!tmp) return 0.0f;

    for (size_t i = 0; i < n; i++) tmp[i] = x[a + i];
    qsort(tmp, n, sizeof(float), otdr_cmp_float_asc);
    float med = (n & 1u) ? tmp[n / 2] : 0.5f * (tmp[n / 2 - 1] + tmp[n / 2]);

    for (size_t i = 0; i < n; i++) tmp[i] = fabsf(tmp[i] - med);
    qsort(tmp, n, sizeof(float), otdr_cmp_float_asc);
    float mad = (n & 1u) ? tmp[n / 2] : 0.5f * (tmp[n / 2 - 1] + tmp[n / 2]);

    free(tmp);
    return mad_scale * mad;
}

/* Trailing RMS envelope */
static void sliding_rms(const float *x, size_t n, size_t win, float *out) {
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

/* Local sigma with stride */
static void compute_local_sigma(const float *delta, size_t n,
                                 size_t win, size_t stride,
                                 float mad_scale, float smin,
                                 float *out) {
    if (!delta || !out || n == 0) return;
    if (win == 0) win = 2048;
    if (stride == 0) stride = 256;

    size_t i = 0;
    while (i < n) {
        size_t start = (i + 1 > win) ? (i + 1 - win) : 0;
        float sigma = robust_sigma_slice(delta, start, i, mad_scale);
        if (sigma < smin) sigma = smin;

        size_t j_end = i + stride;
        if (j_end > n) j_end = n;
        for (size_t j = i; j < j_end; j++) out[j] = sigma;
        i += stride;
    }
}

/* ════════════════════════════════════════════════════════════════════
 * Polarity pattern detection
 * ════════════════════════════════════════════════════════════════════ */

static otdr8_polarity_t detect_polarity(const float *delta, size_t n,
                                         size_t idx, size_t halfwin,
                                         float sigma) {
    if (halfwin == 0) halfwin = 5;
    float thr = sigma * 3.0f;  /* significance threshold */

    /* Gather signed deltas in neighborhood */
    size_t lo = (idx > halfwin) ? (idx - halfwin) : 0;
    size_t hi = (idx + halfwin < n) ? (idx + halfwin) : (n - 1);
    size_t len = hi - lo + 1;
    if (len < 3) return OTDR8_PAT_FLAT;

    int pos_count = 0, neg_count = 0, sign_changes = 0;
    int prev_sign = 0;

    for (size_t i = lo; i <= hi; i++) {
        int s = (delta[i] > thr) ? 1 : ((delta[i] < -thr) ? -1 : 0);
        if (s > 0) pos_count++;
        if (s < 0) neg_count++;
        if (s != 0 && prev_sign != 0 && s != prev_sign) sign_changes++;
        if (s != 0) prev_sign = s;
    }

    int total_sig = pos_count + neg_count;
    if (total_sig == 0) return OTDR8_PAT_FLAT;

    /* Oscillation: many sign changes */
    if (sign_changes >= 3 && total_sig >= 4)
        return OTDR8_PAT_OSCILLATE;

    /* Check center delta for spike vs step */
    float d_center = delta[idx];
    int center_pos = (d_center > thr);
    int center_neg = (d_center < -thr);

    /* Spike+step: positive center followed by negatives */
    if (center_pos && neg_count >= 2 && pos_count <= 3)
        return OTDR8_PAT_SPIKE_STEP;

    /* Positive spike: strong positive, few negatives */
    if (center_pos && pos_count >= 1 && neg_count <= 1)
        return OTDR8_PAT_SPIKE_POS;

    /* Negative spike: strong negative, few positives */
    if (center_neg && neg_count >= 1 && pos_count <= 1)
        return OTDR8_PAT_SPIKE_NEG;

    /* Step down: mostly negative deltas */
    if (neg_count > pos_count * 2)
        return OTDR8_PAT_STEP_DOWN;

    /* Step up: mostly positive deltas */
    if (pos_count > neg_count * 2)
        return OTDR8_PAT_STEP_UP;

    return OTDR8_PAT_FLAT;
}

/* ════════════════════════════════════════════════════════════════════
 * Public: Defaults
 * ════════════════════════════════════════════════════════════════════ */

otdr8_config_t otdr8_default_config(void) {
    otdr8_config_t c;
    memset(&c, 0, sizeof(c));

    /* 4 scales: fine → coarse */
    c.scale_windows[0] = 128;
    c.scale_windows[1] = 512;
    c.scale_windows[2] = 2048;
    c.scale_windows[3] = 8192;
    c.num_scales = 4;

    c.mad_scale = 1.4826f;
    c.local_sigma_enable = 1;
    c.sigma_window = 4096;
    c.sigma_stride = 256;
    c.sigma_min = 1e-12f;

    c.thr_reflect_snr_db     = 12.0f;
    c.thr_atten_snr_db       = 10.0f;
    c.thr_spike_neg_snr_db   = 12.0f;
    c.thr_gainup_snr_db      = 10.0f;
    c.thr_oscillation_snr_db = 8.0f;
    c.thr_broadloss_snr_db   = 6.0f;
    c.min_env_rms = 1e-4f;

    c.polarity_halfwin = 5;

    c.snr_floor_db = -60.0f;
    c.snr_ceil_db  =  60.0f;

    return c;
}

otdr8_merge_config_t otdr8_default_merge_config(void) {
    otdr8_merge_config_t m;
    memset(&m, 0, sizeof(m));

    /* Rule 0: REFLECTION + ATTENUATION → REFLECT_LOSS (v2 compat) */
    m.rules[0].from_a    = OTDR8_EVT_REFLECTION;
    m.rules[0].from_b    = OTDR8_EVT_ATTENUATION;
    m.rules[0].merged_to = OTDR8_EVT_REFLECT_LOSS;
    m.rules[0].max_gap   = 64;
    m.rules[0].min_len_a = 1;
    m.rules[0].min_len_b = 2;
    m.rules[0].min_conf  = 0.0f;

    /* Rule 1: REFLECTION + BROADLOSS → REFLECT_LOSS */
    m.rules[1].from_a    = OTDR8_EVT_REFLECTION;
    m.rules[1].from_b    = OTDR8_EVT_BROADLOSS;
    m.rules[1].merged_to = OTDR8_EVT_REFLECT_LOSS;
    m.rules[1].max_gap   = 32;
    m.rules[1].min_len_a = 1;
    m.rules[1].min_len_b = 3;
    m.rules[1].min_conf  = 0.0f;

    /* Rule 2: adjacent ATTENUATION segments merge */
    m.rules[2].from_a    = OTDR8_EVT_ATTENUATION;
    m.rules[2].from_b    = OTDR8_EVT_ATTENUATION;
    m.rules[2].merged_to = OTDR8_EVT_ATTENUATION;
    m.rules[2].max_gap   = 16;
    m.rules[2].min_len_a = 1;
    m.rules[2].min_len_b = 1;
    m.rules[2].min_conf  = 0.0f;

    /* Rule 3: adjacent OSCILLATION segments merge */
    m.rules[3].from_a    = OTDR8_EVT_OSCILLATION;
    m.rules[3].from_b    = OTDR8_EVT_OSCILLATION;
    m.rules[3].merged_to = OTDR8_EVT_OSCILLATION;
    m.rules[3].max_gap   = 8;
    m.rules[3].min_len_a = 2;
    m.rules[3].min_len_b = 2;
    m.rules[3].min_conf  = 0.0f;

    m.num_rules = 4;
    m.iterative = 1;

    return m;
}

otdr8_passfail_config_t otdr8_default_passfail_config(void) {
    otdr8_passfail_config_t pf;
    pf.max_loss_db        = 1.0f;   /* 1 dB max single-event loss */
    pf.max_reflectance_db = 35.0f;  /* -35 dBc reflectance limit */
    pf.min_snr_db         = 6.0f;   /* minimum acceptable SNR */
    pf.max_event_length   = 500;    /* max samples for single event */
    pf.warn_factor        = 0.7f;   /* warn at 70% of fail threshold */
    return pf;
}

/* ════════════════════════════════════════════════════════════════════
 * Feature extraction
 * ════════════════════════════════════════════════════════════════════ */

int otdr8_extract_features(const float *amp, size_t n,
                           const otdr8_config_t *cfg,
                           otdr8_features_t *out) {
    if (!amp || !cfg || !out || n == 0) return -1;

    size_t ns = cfg->num_scales;
    if (ns == 0 || ns > OTDR_V8_MAX_SCALES) return -1;

    /* Allocate work buffers */
    float *delta = (float *)calloc(n, sizeof(float));
    float *sigma = (float *)calloc(n, sizeof(float));
    float *env   = (float *)calloc(n, sizeof(float));
    if (!delta || !sigma || !env) {
        free(delta); free(sigma); free(env);
        return -2;
    }

    /* Delta trace */
    delta[0] = 0.0f;
    for (size_t i = 1; i < n; i++) delta[i] = amp[i] - amp[i - 1];

    /* Local sigma (from delta) */
    if (cfg->local_sigma_enable) {
        compute_local_sigma(delta, n, cfg->sigma_window, cfg->sigma_stride,
                            cfg->mad_scale, cfg->sigma_min, sigma);
    } else {
        float g = robust_sigma_slice(delta, 0, n - 1, cfg->mad_scale);
        if (g < cfg->sigma_min) g = cfg->sigma_min;
        for (size_t i = 0; i < n; i++) sigma[i] = g;
    }

    /* Initialize features */
    for (size_t i = 0; i < n; i++) {
        out[i].amp = amp[i];
        out[i].delta = delta[i];
        out[i].noise_sigma = sigma[i];
        out[i].max_snr_db = cfg->snr_floor_db;
        out[i].best_scale = 0;
        for (size_t s = 0; s < OTDR_V8_MAX_SCALES; s++) {
            out[i].env_rms[s] = 0.0f;
            out[i].snr_db[s] = cfg->snr_floor_db;
        }
    }

    /* Multi-scale envelope + SNR */
    for (size_t s = 0; s < ns; s++) {
        sliding_rms(amp, n, cfg->scale_windows[s], env);

        for (size_t i = 0; i < n; i++) {
            out[i].env_rms[s] = env[i];

            float d = fabsf(delta[i]);
            float sig = sigma[i];
            if (sig < cfg->sigma_min) sig = cfg->sigma_min;

            float snr = 20.0f * log10f(d / sig);
            if (!isfinite(snr)) snr = cfg->snr_floor_db;
            snr = otdr_clampf(snr, cfg->snr_floor_db, cfg->snr_ceil_db);
            out[i].snr_db[s] = snr;

            if (snr > out[i].max_snr_db) {
                out[i].max_snr_db = snr;
                out[i].best_scale = (uint8_t)s;
            }
        }
    }

    /* Polarity patterns */
    for (size_t i = 0; i < n; i++) {
        out[i].polarity = detect_polarity(delta, n, i,
                                           cfg->polarity_halfwin,
                                           sigma[i]);
    }

    free(delta);
    free(sigma);
    free(env);
    return 0;
}

/* ════════════════════════════════════════════════════════════════════
 * Classification
 * ════════════════════════════════════════════════════════════════════ */

int otdr8_classify(const otdr8_features_t *feat, size_t n,
                   const otdr8_config_t *cfg,
                   otdr8_result_t *out) {
    if (!feat || !cfg || !out || n == 0) return -1;

    for (size_t i = 0; i < n; i++) {
        otdr8_event_t lab = OTDR8_EVT_NONE;
        float conf = 0.0f;

        float snr = feat[i].max_snr_db;
        float d = feat[i].delta;
        /* Check if any scale has enough signal */
        float max_rms = 0.0f;
        for (size_t s = 0; s < cfg->num_scales; s++) {
            if (feat[i].env_rms[s] > max_rms)
                max_rms = feat[i].env_rms[s];
        }

        if (max_rms >= cfg->min_env_rms) {
            otdr8_polarity_t pol = feat[i].polarity;

            /* Use polarity + SNR for classification */
            if (pol == OTDR8_PAT_OSCILLATE && snr >= cfg->thr_oscillation_snr_db) {
                lab = OTDR8_EVT_OSCILLATION;
                conf = otdr_clampf((snr - cfg->thr_oscillation_snr_db) / 20.0f, 0.0f, 1.0f);
            } else if (pol == OTDR8_PAT_SPIKE_STEP && d > 0 && snr >= cfg->thr_reflect_snr_db) {
                /* Connector-like: will be merged later, classify as REFLECTION first */
                lab = OTDR8_EVT_REFLECTION;
                conf = otdr_clampf((snr - cfg->thr_reflect_snr_db) / 20.0f, 0.0f, 1.0f);
            } else if (d > 0 && snr >= cfg->thr_reflect_snr_db) {
                lab = OTDR8_EVT_REFLECTION;
                conf = otdr_clampf((snr - cfg->thr_reflect_snr_db) / 20.0f, 0.0f, 1.0f);
            } else if (d < 0 && snr >= cfg->thr_atten_snr_db) {
                /* Distinguish step from broad loss based on scale */
                if (feat[i].best_scale >= 2 && snr < cfg->thr_atten_snr_db + 6.0f) {
                    lab = OTDR8_EVT_BROADLOSS;
                    conf = otdr_clampf((snr - cfg->thr_broadloss_snr_db) / 20.0f, 0.0f, 1.0f);
                } else {
                    lab = OTDR8_EVT_ATTENUATION;
                    conf = otdr_clampf((snr - cfg->thr_atten_snr_db) / 20.0f, 0.0f, 1.0f);
                }
            } else if (d > 0 && snr >= cfg->thr_gainup_snr_db &&
                       pol == OTDR8_PAT_STEP_UP) {
                lab = OTDR8_EVT_GAINUP;
                conf = otdr_clampf((snr - cfg->thr_gainup_snr_db) / 20.0f, 0.0f, 1.0f);
            } else if (d < 0 && pol == OTDR8_PAT_SPIKE_NEG &&
                       snr >= cfg->thr_spike_neg_snr_db) {
                lab = OTDR8_EVT_SPIKE_NEG;
                conf = otdr_clampf((snr - cfg->thr_spike_neg_snr_db) / 20.0f, 0.0f, 1.0f);
            }
        }

        out[i].label = lab;
        out[i].confidence = conf;
    }
    return 0;
}

/* Full pipeline */
int otdr8_detect(const float *amp, size_t n,
                 const otdr8_config_t *cfg,
                 otdr8_features_t *out_feat,
                 otdr8_result_t *out_res) {
    if (!amp || !cfg || !out_res || n == 0) return -1;

    otdr8_features_t *feat = out_feat;
    if (!feat) {
        feat = (otdr8_features_t *)calloc(n, sizeof(otdr8_features_t));
        if (!feat) return -2;
    }

    int rc = otdr8_extract_features(amp, n, cfg, feat);
    if (rc != 0) { if (!out_feat) free(feat); return rc; }

    rc = otdr8_classify(feat, n, cfg, out_res);
    if (!out_feat) free(feat);
    return rc;
}

/* ════════════════════════════════════════════════════════════════════
 * RLE + Smart Merge
 * ════════════════════════════════════════════════════════════════════ */

/* Internal raw segment (no merge) */
typedef struct {
    size_t start, end;
    otdr8_event_t label;
    float conf_sum;
    size_t count;
    uint32_t flags;
} raw_seg_t;

static size_t build_raw_rle(const otdr8_result_t *res, size_t n,
                             raw_seg_t *out, size_t max_out) {
    if (!res || !out || n == 0 || max_out == 0) return 0;

    size_t out_n = 0;
    size_t start = 0;
    otdr8_event_t cur = res[0].label;
    float csum = res[0].confidence;
    size_t cnt = 1;

    for (size_t i = 1; i < n; i++) {
        if (res[i].label == cur) {
            csum += res[i].confidence;
            cnt++;
        } else {
            if (out_n < max_out) {
                out[out_n].start = start;
                out[out_n].end = i - 1;
                out[out_n].label = cur;
                out[out_n].conf_sum = csum;
                out[out_n].count = cnt;
                out[out_n].flags = 0;
                out_n++;
            }
            start = i;
            cur = res[i].label;
            csum = res[i].confidence;
            cnt = 1;
        }
    }
    if (out_n < max_out) {
        out[out_n].start = start;
        out[out_n].end = n - 1;
        out[out_n].label = cur;
        out[out_n].conf_sum = csum;
        out[out_n].count = cnt;
        out[out_n].flags = 0;
        out_n++;
    }
    return out_n;
}

/* Apply one merge pass using rules. Returns new segment count. */
static size_t merge_pass(raw_seg_t *segs, size_t ns,
                          const otdr8_merge_config_t *mcfg,
                          raw_seg_t *out, size_t max_out) {
    size_t out_n = 0;
    size_t i = 0;

    while (i < ns && out_n < max_out) {
        int merged = 0;

        for (size_t r = 0; r < mcfg->num_rules && !merged; r++) {
            const otdr8_merge_rule_t *rule = &mcfg->rules[r];
            if (segs[i].label != rule->from_a) continue;

            size_t len_a = segs[i].end - segs[i].start + 1;
            if (len_a < rule->min_len_a) continue;

            float conf_a = segs[i].conf_sum / (float)segs[i].count;
            if (conf_a < rule->min_conf) continue;

            /* Look ahead past NONE-gaps */
            size_t j = i + 1;
            size_t gap = 0;
            while (j < ns && segs[j].label == OTDR8_EVT_NONE) {
                gap += segs[j].end - segs[j].start + 1;
                if (gap > rule->max_gap) break;
                j++;
            }

            if (j < ns && gap <= rule->max_gap &&
                segs[j].label == rule->from_b) {
                size_t len_b = segs[j].end - segs[j].start + 1;
                float conf_b = segs[j].conf_sum / (float)segs[j].count;

                if (len_b >= rule->min_len_b && conf_b >= rule->min_conf) {
                    /* Merge i..j into one segment */
                    raw_seg_t m;
                    m.start = segs[i].start;
                    m.end = segs[j].end;
                    m.label = rule->merged_to;
                    m.conf_sum = segs[i].conf_sum + segs[j].conf_sum;
                    m.count = (m.end - m.start + 1);
                    m.flags = 1;  /* merged */

                    out[out_n++] = m;
                    i = j + 1;
                    merged = 1;
                }
            }
        }

        if (!merged) {
            out[out_n++] = segs[i];
            i++;
        }
    }
    return out_n;
}

size_t otdr8_segment_merge(const otdr8_result_t *res,
                           const otdr8_features_t *feat,
                           size_t n,
                           const otdr8_merge_config_t *mcfg,
                           otdr8_segment_t *seg_out,
                           size_t max_seg) {
    if (!res || !seg_out || n == 0 || max_seg == 0) return 0;

    otdr8_merge_config_t mc = mcfg ? *mcfg : otdr8_default_merge_config();

    /* Build raw RLE */
    size_t max_raw = 16384;
    raw_seg_t *buf_a = (raw_seg_t *)calloc(max_raw, sizeof(raw_seg_t));
    raw_seg_t *buf_b = (raw_seg_t *)calloc(max_raw, sizeof(raw_seg_t));
    if (!buf_a || !buf_b) { free(buf_a); free(buf_b); return 0; }

    size_t ns = build_raw_rle(res, n, buf_a, max_raw);

    /* Iterative merge */
    int max_iter = mc.iterative ? 8 : 1;
    for (int iter = 0; iter < max_iter; iter++) {
        size_t new_ns = merge_pass(buf_a, ns, &mc, buf_b, max_raw);
        if (new_ns == ns) break;  /* stable */

        /* Swap buffers */
        raw_seg_t *tmp = buf_a;
        buf_a = buf_b;
        buf_b = tmp;
        ns = new_ns;
    }

    /* Convert raw segments to output, filling metrics from features */
    size_t out_n = 0;
    for (size_t i = 0; i < ns && out_n < max_seg; i++) {
        otdr8_segment_t *s = &seg_out[out_n];
        s->start = buf_a[i].start;
        s->end   = buf_a[i].end;
        s->label = buf_a[i].label;
        s->mean_conf = buf_a[i].conf_sum / (float)buf_a[i].count;
        s->flags = buf_a[i].flags;
        s->verdict = OTDR8_VERDICT_PASS;
        s->fail_reasons = OTDR8_FAIL_NONE;

        /* Compute metrics from features if available */
        s->peak_snr_db = -100.0f;
        s->peak_amplitude = 0.0f;
        s->dominant_polarity = OTDR8_PAT_FLAT;

        if (feat) {
            int pol_counts[7] = {0};
            for (size_t j = s->start; j <= s->end && j < n; j++) {
                if (feat[j].max_snr_db > s->peak_snr_db)
                    s->peak_snr_db = feat[j].max_snr_db;
                float ad = fabsf(feat[j].delta);
                if (ad > s->peak_amplitude)
                    s->peak_amplitude = ad;
                int pi = (int)feat[j].polarity;
                if (pi >= 0 && pi < 7) pol_counts[pi]++;
            }
            /* Dominant polarity */
            int best_pi = 0, best_cnt = 0;
            for (int p = 0; p < 7; p++) {
                if (pol_counts[p] > best_cnt) {
                    best_cnt = pol_counts[p];
                    best_pi = p;
                }
            }
            s->dominant_polarity = (otdr8_polarity_t)best_pi;
        }

        out_n++;
    }

    free(buf_a);
    free(buf_b);
    return out_n;
}

/* ════════════════════════════════════════════════════════════════════
 * Pass/Fail
 * ════════════════════════════════════════════════════════════════════ */

void otdr8_apply_passfail(otdr8_segment_t *segs, size_t nseg,
                          const otdr8_passfail_config_t *pf) {
    if (!segs || !pf || nseg == 0) return;

    otdr8_passfail_config_t p = *pf;

    for (size_t i = 0; i < nseg; i++) {
        if (segs[i].label == OTDR8_EVT_NONE) {
            segs[i].verdict = OTDR8_VERDICT_PASS;
            segs[i].fail_reasons = OTDR8_FAIL_NONE;
            continue;
        }

        uint32_t reasons = OTDR8_FAIL_NONE;
        int is_warn = 0;

        /* Loss check (amplitude as proxy for dB loss) */
        float loss_proxy = segs[i].peak_amplitude;
        if (loss_proxy > p.max_loss_db) {
            reasons |= OTDR8_FAIL_HIGH_LOSS;
        } else if (loss_proxy > p.max_loss_db * p.warn_factor) {
            is_warn = 1;
        }

        /* SNR check */
        if (segs[i].peak_snr_db < p.min_snr_db) {
            reasons |= OTDR8_FAIL_LOW_SNR;
        } else if (segs[i].peak_snr_db < p.min_snr_db / p.warn_factor) {
            is_warn = 1;
        }

        /* Length check */
        size_t len = segs[i].end - segs[i].start + 1;
        if (len > p.max_event_length) {
            reasons |= OTDR8_FAIL_TOO_LONG;
        }

        /* Reflectance check (for reflection-type events) */
        if (segs[i].label == OTDR8_EVT_REFLECTION ||
            segs[i].label == OTDR8_EVT_REFLECT_LOSS) {
            if (segs[i].peak_snr_db > p.max_reflectance_db) {
                reasons |= OTDR8_FAIL_HIGH_REFL;
            }
        }

        /* Pattern check */
        if (segs[i].dominant_polarity == OTDR8_PAT_OSCILLATE) {
            reasons |= OTDR8_FAIL_PATTERN;
        }

        segs[i].fail_reasons = reasons;
        if (reasons != OTDR8_FAIL_NONE)
            segs[i].verdict = OTDR8_VERDICT_FAIL;
        else if (is_warn)
            segs[i].verdict = OTDR8_VERDICT_WARN;
        else
            segs[i].verdict = OTDR8_VERDICT_PASS;
    }
}

/* ════════════════════════════════════════════════════════════════════
 * String helpers
 * ════════════════════════════════════════════════════════════════════ */

const char *otdr8_event_str(otdr8_event_t e) {
    switch (e) {
    case OTDR8_EVT_NONE:         return "NONE";
    case OTDR8_EVT_REFLECTION:   return "REFLECTION";
    case OTDR8_EVT_ATTENUATION:  return "ATTENUATION";
    case OTDR8_EVT_REFLECT_LOSS: return "REFLECT_LOSS";
    case OTDR8_EVT_GAINUP:       return "GAIN_UP";
    case OTDR8_EVT_SPIKE_NEG:    return "SPIKE_NEG";
    case OTDR8_EVT_OSCILLATION:  return "OSCILLATION";
    case OTDR8_EVT_BROADLOSS:    return "BROAD_LOSS";
    default:                     return "UNKNOWN";
    }
}

const char *otdr8_polarity_str(otdr8_polarity_t p) {
    switch (p) {
    case OTDR8_PAT_FLAT:       return "FLAT";
    case OTDR8_PAT_SPIKE_POS:  return "SPIKE_POS";
    case OTDR8_PAT_SPIKE_NEG:  return "SPIKE_NEG";
    case OTDR8_PAT_STEP_DOWN:  return "STEP_DOWN";
    case OTDR8_PAT_STEP_UP:    return "STEP_UP";
    case OTDR8_PAT_SPIKE_STEP: return "SPIKE_STEP";
    case OTDR8_PAT_OSCILLATE:  return "OSCILLATE";
    default:                   return "UNKNOWN";
    }
}

const char *otdr8_verdict_str(otdr8_verdict_t v) {
    switch (v) {
    case OTDR8_VERDICT_PASS: return "PASS";
    case OTDR8_VERDICT_WARN: return "WARN";
    case OTDR8_VERDICT_FAIL: return "FAIL";
    default:                 return "UNKNOWN";
    }
}
