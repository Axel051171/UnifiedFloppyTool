#include "tdfc.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

static float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static void compute_mean_std(const float *x, size_t n, float *mean, float *stddev) {
    if (!x || n == 0) { *mean = 0.0f; *stddev = 0.0f; return; }
    double m = 0.0, s = 0.0;
    for (size_t i = 0; i < n; i++) m += x[i];
    m /= (double)n;
    for (size_t i = 0; i < n; i++) {
        double d = (double)x[i] - m;
        s += d * d;
    }
    s = (n > 1) ? (s / (double)(n - 1)) : 0.0;
    *mean = (float)m;
    *stddev = (float)sqrt(s);
}

static void sliding_rms(const float *x, size_t n,
                        size_t win, size_t step,
                        float *out, size_t n_points)
{
    if (n == 0 || win == 0) { for (size_t i = 0; i < n_points; i++) out[i] = 0.0f; return; }
    double sumsq = 0.0;
    size_t a = 0, b = 0;

    for (size_t p = 0; p < n_points; p++) {
        size_t idx = p * step;
        if (idx >= n) idx = n - 1;

        size_t end = idx + 1;
        size_t start = (end > win) ? (end - win) : 0;

        while (b < end) { double v = x[b]; sumsq += v * v; b++; }
        while (a < start) { double v = x[a]; sumsq -= v * v; a++; }

        size_t cur = (b > a) ? (b - a) : 1;
        out[p] = (float)sqrt(sumsq / (double)cur);
    }
}

static void sliding_snr_db(const float *x, size_t n,
                           size_t win, size_t step,
                           float *out, size_t n_points)
{
    if (n == 0 || win == 0) { for (size_t i = 0; i < n_points; i++) out[i] = 0.0f; return; }
    double sum = 0.0, sumsq = 0.0;
    size_t a = 0, b = 0;

    for (size_t p = 0; p < n_points; p++) {
        size_t idx = p * step;
        if (idx >= n) idx = n - 1;

        size_t end = idx + 1;
        size_t start = (end > win) ? (end - win) : 0;

        while (b < end) { double v = fabs((double)x[b]); sum += v; sumsq += v * v; b++; }
        while (a < start) { double v = fabs((double)x[a]); sum -= v; sumsq -= v * v; a++; }

        size_t cur = (b > a) ? (b - a) : 1;
        double mean = sum / (double)cur;

        double var = 0.0;
        if (cur > 1) {
            var = (sumsq - (sum * sum) / (double)cur) / (double)(cur - 1);
            if (var < 0.0) var = 0.0;
        }
        double sd = sqrt(var);

        double ratio = (sd > 1e-12) ? (mean / sd) : 0.0;
        out[p] = (float)((ratio > 1e-12) ? (20.0 * log10(ratio)) : -120.0);
    }
}

static float norm_corr_at(const float *x, size_t n,
                          const float *t, size_t L,
                          size_t center)
{
    if (!x || !t || n == 0 || L == 0) return 0.0f;
    size_t end = center + 1;
    if (end < L) return 0.0f;
    if (end > n) end = n;
    size_t start = end - L;

    double dot = 0.0, nx = 0.0, nt = 0.0;
    for (size_t i = 0; i < L; i++) {
        double xv = x[start + i];
        double tv = t[i];
        dot += xv * tv;
        nx += xv * xv;
        nt += tv * tv;
    }
    double denom = sqrt(nx) * sqrt(nt);
    if (denom < 1e-18) return 0.0f;
    return (float)(dot / denom);
}

static void cusum_changepoints(const float *s, size_t n_points,
                               float k, float h,
                               uint8_t *change_out)
{
    if (!s || !change_out || n_points == 0) return;
    for (size_t i = 0; i < n_points; i++) change_out[i] = 0;

    double mean = 0.0;
    for (size_t i = 0; i < n_points; i++) mean += (double)s[i];
    mean /= (double)n_points;

    double gp = 0.0, gn = 0.0;
    for (size_t i = 0; i < n_points; i++) {
        double x = (double)s[i] - mean;
        gp = fmax(0.0, gp + x - (double)k);
        gn = fmin(0.0, gn + x + (double)k);

        if (gp > (double)h || gn < -(double)h) {
            change_out[i] = 1;
            gp = 0.0;
            gn = 0.0;
        }
    }
}

tdfc_config tdfc_default_config(void) {
    tdfc_config c;
    c.env_window = 512;
    c.snr_window = 1024;
    c.step = 64;
    c.cusum_k = 0.05f;
    c.cusum_h = 6.0f;
    c.remove_dc = 1;
    c.template_sig = NULL;
    c.template_len = 0;
    return c;
}

void tdfc_free_result(tdfc_result *r) {
    if (!r) return;
    free(r->envelope_rms); r->envelope_rms = NULL;
    free(r->snr_db);       r->snr_db = NULL;
    free(r->corr);         r->corr = NULL;
    free(r->change);       r->change = NULL;
    r->n_points = 0;
    r->step = 0;
    r->global_mean = 0.0f;
    r->global_std = 0.0f;
}

void tdfc_i16_to_f32(const int16_t *in, float *out, size_t n, float scale) {
    if (!in || !out || n == 0) return;
    if (scale == 0.0f) scale = 32768.0f;
    for (size_t i = 0; i < n; i++) out[i] = (float)in[i] / scale;
}

static int validate_cfg(const tdfc_config *cfg) {
    if (!cfg) return -1;
    if (cfg->step == 0) return -2;
    if (cfg->env_window == 0) return -3;
    if (cfg->snr_window == 0) return -4;
    if (cfg->cusum_h <= 0.0f) return -5;
    if (cfg->cusum_k < 0.0f) return -6;
    if ((cfg->template_sig == NULL) != (cfg->template_len == 0)) return -7;
    return 0;
}

int tdfc_analyze(const float *signal, size_t n_samples,
                 const tdfc_config *cfg, tdfc_result *out)
{
    if (!out) return -1;
    memset(out, 0, sizeof(*out));

    int v = validate_cfg(cfg);
    if (v != 0) return v;
    if (!signal || n_samples == 0) return -10;

    const size_t step = cfg->step;
    const size_t n_points = ((n_samples - 1) / step + 1);

    out->n_points = n_points;
    out->step = step;

    out->envelope_rms = (float*)calloc(n_points, sizeof(float));
    out->snr_db       = (float*)calloc(n_points, sizeof(float));
    out->change       = (uint8_t*)calloc(n_points, sizeof(uint8_t));
    out->corr         = NULL;

    if (!out->envelope_rms || !out->snr_db || !out->change) {
        tdfc_free_result(out);
        return -20;
    }

    float *x = (float*)malloc(n_samples * sizeof(float));
    if (!x) { tdfc_free_result(out); return -21; }
    memcpy(x, signal, n_samples * sizeof(float));

    float mean = 0.0f, sd = 0.0f;
    compute_mean_std(x, n_samples, &mean, &sd);
    out->global_mean = mean;
    out->global_std = sd;

    if (cfg->remove_dc) {
        for (size_t i = 0; i < n_samples; i++) x[i] -= mean;
        compute_mean_std(x, n_samples, &mean, &sd);
        out->global_mean = mean;
        out->global_std = sd;
    }

    sliding_rms(x, n_samples, cfg->env_window, step, out->envelope_rms, n_points);
    sliding_snr_db(x, n_samples, cfg->snr_window, step, out->snr_db, n_points);

    if (cfg->template_sig && cfg->template_len > 0) {
        out->corr = (float*)calloc(n_points, sizeof(float));
        if (!out->corr) {
            free(x);
            tdfc_free_result(out);
            return -22;
        }
        for (size_t p = 0; p < n_points; p++) {
            size_t idx = p * step;
            if (idx >= n_samples) idx = n_samples - 1;
            out->corr[p] = norm_corr_at(x, n_samples, cfg->template_sig, cfg->template_len, idx);
        }
    }

    cusum_changepoints(out->snr_db, n_points, cfg->cusum_k, cfg->cusum_h, out->change);

    free(x);
    return 0;
}

static int cmp_float_asc_tdfc(const void *a, const void *b) {
    float fa = *(const float*)a, fb = *(const float*)b;
    return (fa < fb) ? -1 : (fa > fb);
}

int tdfc_health_score(const tdfc_result *r) {
    if (!r || r->n_points == 0 || !r->snr_db || !r->change) return 0;
    size_t n = r->n_points;

    float *tmp = (float*)malloc(n * sizeof(float));
    if (!tmp) return 0;
    for (size_t i = 0; i < n; i++) tmp[i] = r->snr_db[i];

    qsort(tmp, n, sizeof(float), cmp_float_asc_tdfc);

    size_t lo = n / 10, hi = n - n / 10;
    if (hi <= lo) { lo = 0; hi = n; }

    double sum = 0.0; size_t cnt = 0;
    for (size_t i = lo; i < hi; i++) { sum += tmp[i]; cnt++; }
    free(tmp);

    double snr = cnt ? (sum / (double)cnt) : -120.0;

    double changes = 0.0;
    for (size_t i = 0; i < n; i++) if (r->change[i]) changes += 1.0;
    double change_rate = changes / (double)n;

    double snr_score = (snr + 5.0) / 25.0;
    snr_score = clampf((float)snr_score, 0.0f, 1.0f);

    double change_penalty = 1.0 - (change_rate / 0.05);
    if (change_penalty < 0.0) change_penalty = 0.0;
    if (change_penalty > 1.0) change_penalty = 1.0;

    double score = 100.0 * (0.75 * snr_score + 0.25 * change_penalty);
    if (score < 0.0) score = 0.0;
    if (score > 100.0) score = 100.0;
    return (int)(score + 0.5);
}
