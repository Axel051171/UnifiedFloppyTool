#include "tdfc.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---- sorting ---- */
static int cmp_float_asc(const void *a, const void *b) {
    float fa = *(const float*)a, fb = *(const float*)b;
    return (fa < fb) ? -1 : (fa > fb);
}

static float mean_range(const float *x, size_t a, size_t b_incl) {
    if (!x || b_incl < a) return 0.0f;
    double sum = 0.0;
    size_t n = b_incl - a + 1;
    for (size_t i = a; i <= b_incl; i++) sum += (double)x[i];
    return (float)(sum / (double)n);
}

static float mean_u8_range_as_ratio(const uint8_t *x, size_t a, size_t b_incl) {
    if (!x || b_incl < a) return 0.0f;
    double sum = 0.0;
    size_t n = b_incl - a + 1;
    for (size_t i = a; i <= b_incl; i++) sum += (double)(x[i] ? 1.0 : 0.0);
    return (float)(sum / (double)n);
}

/* ---- robust stats ---- */
int tdfc_compute_robust_stats(const float *x, size_t n, float trim_frac, tdfc_robust_stats_t *out) {
    if (!out) return -1;
    memset(out, 0, sizeof(*out));
    if (!x || n == 0) return -2;
    if (trim_frac < 0.0f) trim_frac = 0.0f;
    if (trim_frac > 0.49f) trim_frac = 0.49f;

    float *tmp = (float*)malloc(n * sizeof(float));
    if (!tmp) return -3;
    for (size_t i = 0; i < n; i++) tmp[i] = x[i];
    qsort(tmp, n, sizeof(float), cmp_float_asc);

    float med = 0.0f;
    if (n & 1u) med = tmp[n/2];
    else med = 0.5f * (tmp[n/2 - 1] + tmp[n/2]);
    out->median = med;

    size_t k = (size_t)floor((double)n * (double)trim_frac);
    size_t lo = k;
    size_t hi = (n > k) ? (n - 1 - k) : 0;
    if (hi < lo) { lo = 0; hi = n - 1; }

    double sum = 0.0; size_t cnt = 0;
    for (size_t i = lo; i <= hi; i++) { sum += (double)tmp[i]; cnt++; }
    out->trimmed_mean = (float)(cnt ? (sum / (double)cnt) : 0.0f);

    float *dev = (float*)malloc(n * sizeof(float));
    if (!dev) { free(tmp); return -4; }
    for (size_t i = 0; i < n; i++) dev[i] = fabsf(x[i] - med);
    qsort(dev, n, sizeof(float), cmp_float_asc);

    float mad = 0.0f;
    if (n & 1u) mad = dev[n/2];
    else mad = 0.5f * (dev[n/2 - 1] + dev[n/2]);
    out->mad = mad;
    out->sigma_mad = 1.4826f * mad;

    free(dev);
    free(tmp);
    return 0;
}

/* ---- dropout detection on envelope points ---- */
int tdfc_detect_dropouts_env(const float *envelope_rms, size_t n_points,
                             float threshold, size_t min_run,
                             uint8_t *dropout_flag, float *dropout_ratio)
{
    if (!envelope_rms || n_points == 0 || !dropout_flag) return -1;
    if (min_run == 0) min_run = 1;

    for (size_t i = 0; i < n_points; i++) dropout_flag[i] = 0;

    size_t total_drop = 0;
    size_t run_start = 0;
    size_t run_len = 0;

    for (size_t i = 0; i < n_points; i++) {
        int is_low = (envelope_rms[i] < threshold);

        if (is_low) {
            if (run_len == 0) run_start = i;
            run_len++;
        } else {
            if (run_len >= min_run) {
                for (size_t j = run_start; j < run_start + run_len; j++) {
                    dropout_flag[j] = 1;
                    total_drop++;
                }
            }
            run_len = 0;
        }
    }

    if (run_len >= min_run) {
        for (size_t j = run_start; j < run_start + run_len; j++) {
            dropout_flag[j] = 1;
            total_drop++;
        }
    }

    if (dropout_ratio) *dropout_ratio = (float)total_drop / (float)n_points;
    return 0;
}

/* ---- dropout detection on raw amplitude samples ---- */
int tdfc_detect_dropouts_amp(const float *signal, size_t n_samples,
                             size_t step, float threshold, size_t min_run_samples,
                             uint8_t *dropout_flag_samples,
                             uint8_t *dropout_flag_points,
                             float *dropout_ratio_points)
{
    if (!signal || n_samples == 0 || step == 0 || !dropout_flag_points) return -1;
    if (min_run_samples == 0) min_run_samples = 1;

    size_t n_points = ((n_samples - 1) / step + 1);

    if (dropout_flag_samples) memset(dropout_flag_samples, 0, n_samples);
    memset(dropout_flag_points, 0, n_points);

    size_t total_drop_samples = 0;

    size_t run_start = 0;
    size_t run_len = 0;

    for (size_t i = 0; i < n_samples; i++) {
        float v = fabsf(signal[i]);
        int is_low = (v < threshold);

        if (is_low) {
            if (run_len == 0) run_start = i;
            run_len++;
        } else {
            if (run_len >= min_run_samples) {
                /* mark samples */
                if (dropout_flag_samples) {
                    for (size_t j = run_start; j < run_start + run_len; j++) dropout_flag_samples[j] = 1;
                }
                total_drop_samples += run_len;

                /* map to points: mark any point whose sample index is in [run_start, run_end] */
                size_t run_end = run_start + run_len - 1;
                size_t p0 = run_start / step;
                size_t p1 = run_end / step;
                if (p1 >= n_points) p1 = n_points - 1;
                for (size_t p = p0; p <= p1; p++) dropout_flag_points[p] = 1;
            }
            run_len = 0;
        }
    }

    /* tail */
    if (run_len >= min_run_samples) {
        if (dropout_flag_samples) {
            for (size_t j = run_start; j < run_start + run_len; j++) dropout_flag_samples[j] = 1;
        }
        total_drop_samples += run_len;

        size_t run_end = run_start + run_len - 1;
        size_t p0 = run_start / step;
        size_t p1 = run_end / step;
        if (p1 >= n_points) p1 = n_points - 1;
        for (size_t p = p0; p <= p1; p++) dropout_flag_points[p] = 1;
    }

    if (dropout_ratio_points) {
        size_t total_drop_points = 0;
        for (size_t p = 0; p < n_points; p++) if (dropout_flag_points[p]) total_drop_points++;
        *dropout_ratio_points = (float)total_drop_points / (float)n_points;
    }
    return 0;
}

/* ---- segmentation ---- */
static void free_seg(tdfc_segmentation *s) {
    if (!s) return;
    free(s->seg);
    s->seg = NULL;
    s->n_seg = 0;
}

void tdfc_free_segmentation(tdfc_segmentation *s) { free_seg(s); }

static int append_segment(tdfc_segmentation *out, const tdfc_segment *seg) {
    size_t new_n = out->n_seg + 1;
    tdfc_segment *ns = (tdfc_segment*)realloc(out->seg, new_n * sizeof(tdfc_segment));
    if (!ns) return -1;
    out->seg = ns;
    out->seg[out->n_seg] = *seg;
    out->n_seg = new_n;
    return 0;
}

static void compute_segment_metrics(const tdfc_result *r,
                                    const uint8_t *dropout_flag_points,
                                    size_t a, size_t b,
                                    tdfc_segment *s)
{
    memset(s, 0, sizeof(*s));
    s->start_point = a;
    s->end_point = b;

    s->mean_snr_db = r->snr_db ? mean_range(r->snr_db, a, b) : 0.0f;
    s->mean_env_rms = r->envelope_rms ? mean_range(r->envelope_rms, a, b) : 0.0f;
    s->dropout_rate = dropout_flag_points ? mean_u8_range_as_ratio(dropout_flag_points, a, b) : 0.0f;

    s->flags = TDFC_SEG_OK;
    if (s->dropout_rate > 0.0f) s->flags |= TDFC_SEG_FLAG_HAS_DROPOUTS;
    if (s->mean_snr_db < 6.0f || s->dropout_rate > 0.02f) s->flags |= TDFC_SEG_FLAG_DEGRADED;

    float snr = s->mean_snr_db;
    float s_snr = (snr - 0.0f) / 20.0f;
    if (s_snr < 0.0f) s_snr = 0.0f;
    if (s_snr > 1.0f) s_snr = 1.0f;

    float p_do = s->dropout_rate / 0.05f;
    if (p_do < 0.0f) p_do = 0.0f;
    if (p_do > 1.0f) p_do = 1.0f;

    s->score = 100.0f * (0.75f * s_snr + 0.25f * (1.0f - p_do));
}

static size_t seg_len(const tdfc_segment *s) {
    if (!s) return 0;
    if (s->end_point < s->start_point) return 0;
    return (s->end_point - s->start_point + 1);
}

int tdfc_segment_from_changepoints(const tdfc_result *r,
                                   const uint8_t *dropout_flag_points,
                                   size_t min_seg_len,
                                   tdfc_segmentation *out)
{
    if (!out) return -1;
    memset(out, 0, sizeof(*out));
    if (!r || r->n_points == 0 || !r->change) return -2;
    if (min_seg_len == 0) min_seg_len = 1;

    size_t start = 0;
    for (size_t i = 0; i < r->n_points; i++) {
        int is_cut = (r->change[i] != 0) && (i != start);
        int is_last = (i == r->n_points - 1);

        if (is_cut || is_last) {
            size_t end = is_last ? i : (i - 1);

            tdfc_segment s;
            compute_segment_metrics(r, dropout_flag_points, start, end, &s);

            if (append_segment(out, &s) != 0) { free_seg(out); return -3; }
            start = i;
        }
    }

    if (out->n_seg == 0) return 0;

    for (;;) {
        int merged_any = 0;
        for (size_t si = 0; si < out->n_seg; si++) {
            if (seg_len(&out->seg[si]) >= min_seg_len) continue;
            if (out->n_seg == 1) break;

            size_t left = (si > 0) ? (si - 1) : (size_t)-1;
            size_t right = (si + 1 < out->n_seg) ? (si + 1) : (size_t)-1;

            size_t merge_to = (size_t)-1;
            if (left != (size_t)-1 && right != (size_t)-1) {
                float dl = fabsf(out->seg[si].mean_snr_db - out->seg[left].mean_snr_db);
                float dr = fabsf(out->seg[si].mean_snr_db - out->seg[right].mean_snr_db);
                merge_to = (dl <= dr) ? left : right;
            } else if (left != (size_t)-1) merge_to = left;
            else merge_to = right;

            if (merge_to == (size_t)-1) continue;

            size_t a = out->seg[merge_to].start_point < out->seg[si].start_point
                       ? out->seg[merge_to].start_point : out->seg[si].start_point;
            size_t b = out->seg[merge_to].end_point > out->seg[si].end_point
                       ? out->seg[merge_to].end_point : out->seg[si].end_point;

            compute_segment_metrics(r, dropout_flag_points, a, b, &out->seg[merge_to]);

            for (size_t k = si + 1; k < out->n_seg; k++) out->seg[k - 1] = out->seg[k];
            out->n_seg--;
            tdfc_segment *ns = (tdfc_segment*)realloc(out->seg, out->n_seg * sizeof(tdfc_segment));
            if (ns || out->n_seg == 0) out->seg = ns;

            merged_any = 1;
            break;
        }
        if (!merged_any) break;
    }

    return 0;
}
