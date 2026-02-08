#include "phi_otdr_denoise_1d.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ----------------- helpers ----------------- */

static float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static int cmp_float_asc(const void *a, const void *b) {
    float fa = *(const float*)a, fb = *(const float*)b;
    return (fa < fb) ? -1 : (fa > fb);
}

static float median_of_copy(const float *x, size_t n) {
    if (!x || n == 0) return 0.0f;
    float *tmp = (float*)malloc(n * sizeof(float));
    if (!tmp) return 0.0f;
    for (size_t i = 0; i < n; i++) tmp[i] = x[i];
    qsort(tmp, n, sizeof(float), cmp_float_asc);
    float med = (n & 1u) ? tmp[n/2] : 0.5f * (tmp[n/2 - 1] + tmp[n/2]);
    free(tmp);
    return med;
}

static float mad_sigma(const float *x, size_t n) {
    if (!x || n == 0) return 0.0f;
    float med = median_of_copy(x, n);
    float *dev = (float*)malloc(n * sizeof(float));
    if (!dev) return 0.0f;
    for (size_t i = 0; i < n; i++) dev[i] = fabsf(x[i] - med);
    qsort(dev, n, sizeof(float), cmp_float_asc);
    float mad = (n & 1u) ? dev[n/2] : 0.5f * (dev[n/2 - 1] + dev[n/2]);
    free(dev);
    return 1.4826f * mad;
}

static float soft_thresh(float v, float t) {
    float a = fabsf(v);
    if (a <= t) return 0.0f;
    return copysignf(a - t, v);
}

static float hard_thresh(float v, float t) {
    return (fabsf(v) <= t) ? 0.0f : v;
}

/* ----------------- Haar SWT ----------------- */

static void swt_haar_forward(const float *x, size_t n, size_t levels,
                             float **approx_out, float **detail_out)
{
    const float inv_sqrt2 = 0.7071067811865475f;
    float *a_prev = (float*)malloc(n * sizeof(float));
    memcpy(a_prev, x, n * sizeof(float));

    for (size_t l = 1; l <= levels; l++) {
        size_t step = (size_t)1u << (l - 1u);
        float *a = approx_out[l - 1];
        float *d = detail_out[l - 1];
        for (size_t i = 0; i < n; i++) {
            size_t j = (i + n - (step % n)) % n;
            float x0 = a_prev[i];
            float x1 = a_prev[j];
            a[i] = (x0 + x1) * inv_sqrt2;
            d[i] = (x0 - x1) * inv_sqrt2;
        }
        memcpy(a_prev, a, n * sizeof(float));
    }

    free(a_prev);
}

static void swt_haar_inverse(float *x_out, size_t n, size_t levels,
                             float **approx, float **detail)
{
    const float inv_sqrt2 = 0.7071067811865475f;
    float *a = (float*)malloc(n * sizeof(float));
    memcpy(a, approx[levels - 1], n * sizeof(float));

    for (size_t l = levels; l >= 1; l--) {
        size_t step = (size_t)1u << (l - 1u);
        float *a_prev = (float*)calloc(n, sizeof(float));
        float *d = detail[l - 1];

        for (size_t i = 0; i < n; i++) {
            float x0 = (a[i] + d[i]) * inv_sqrt2;
            float x1 = (a[i] - d[i]) * inv_sqrt2;
            size_t j = (i + n - (step % n)) % n;
            a_prev[i] += x0;
            a_prev[j] += x1;
        }
        for (size_t i = 0; i < n; i++) a_prev[i] *= 0.5f;

        free(a);
        a = a_prev;

        if (l == 1) break;
    }

    memcpy(x_out, a, n * sizeof(float));
    free(a);
}

/* ----------------- auto quiet mask ----------------- */

typedef struct {
    float var;
    size_t win_index;
} win_var_t;

static int cmp_winvar_asc(const void *a, const void *b) {
    float va = ((const win_var_t*)a)->var;
    float vb = ((const win_var_t*)b)->var;
    return (va < vb) ? -1 : (va > vb);
}

int podr_build_auto_quiet_mask(const float *x, size_t n,
                               size_t window, float keep_frac,
                               uint8_t *quiet_mask_out)
{
    if (!x || !quiet_mask_out || n == 0) return -1;
    if (window == 0) window = 1;
    if (window > n) window = n;
    keep_frac = clampf(keep_frac, 0.01f, 0.99f);

    memset(quiet_mask_out, 0, n);

    size_t n_win = (n + window - 1) / window;
    win_var_t *wv = (win_var_t*)malloc(n_win * sizeof(win_var_t));
    if (!wv) return -2;

    for (size_t w = 0; w < n_win; w++) {
        size_t a = w * window;
        size_t b = a + window;
        if (b > n) b = n;

        double sum = 0.0, sumsq = 0.0;
        size_t cnt = b - a;
        for (size_t i = a; i < b; i++) {
            double v = (double)x[i];
            sum += v;
            sumsq += v * v;
        }
        double mean = sum / (double)cnt;
        double var = (sumsq / (double)cnt) - (mean * mean);
        if (var < 0.0) var = 0.0;

        wv[w].var = (float)var;
        wv[w].win_index = w;
    }

    qsort(wv, n_win, sizeof(win_var_t), cmp_winvar_asc);

    size_t keep = (size_t)floor((double)n_win * (double)keep_frac);
    if (keep < 1) keep = 1;

    for (size_t i = 0; i < keep; i++) {
        size_t w = wv[i].win_index;
        size_t a = w * window;
        size_t b = a + window;
        if (b > n) b = n;
        for (size_t j = a; j < b; j++) quiet_mask_out[j] = 1;
    }

    free(wv);
    return 0;
}

/* ----------------- public API ----------------- */

podr_config podr_default_config(void) {
    podr_config c;
    c.levels = 5;
    c.mode = PODR_THRESH_SOFT;
    c.thresh_scale = 1.0f;
    c.level_gain = NULL;

    c.quiet_mask = NULL;
    c.auto_quiet = 1;
    c.quiet_window = 2048;
    c.quiet_keep_frac = 0.20f;

    c.sigma_override = 0.0f;
    return c;
}

void podr_free_diag(podr_diag *d) {
    if (!d) return;
    free(d->thr_per_level);
    d->thr_per_level = NULL;
    d->sigma_est = 0.0f;
}

static float estimate_sigma_from_quiet_details(const float *detail, const uint8_t *quiet, size_t n) {
    if (!quiet) return mad_sigma(detail, n);

    size_t cnt = 0;
    for (size_t i = 0; i < n; i++) if (quiet[i]) cnt++;
    if (cnt < 16) return mad_sigma(detail, n);

    float *tmp = (float*)malloc(cnt * sizeof(float));
    if (!tmp) return mad_sigma(detail, n);

    size_t k = 0;
    for (size_t i = 0; i < n; i++) if (quiet[i]) tmp[k++] = detail[i];

    float s = mad_sigma(tmp, cnt);
    free(tmp);
    return s;
}

int podr_denoise_1d(const float *in, float *out, size_t n,
                    const podr_config *cfg, podr_diag *diag)
{
    if (!in || !out || !cfg || n == 0) return -1;
    if (cfg->levels == 0) return -2;

    size_t L = cfg->levels;

    float **A = (float**)calloc(L, sizeof(float*));
    float **D = (float**)calloc(L, sizeof(float*));
    if (!A || !D) { free(A); free(D); return -3; }

    for (size_t l = 0; l < L; l++) {
        A[l] = (float*)malloc(n * sizeof(float));
        D[l] = (float*)malloc(n * sizeof(float));
        if (!A[l] || !D[l]) {
            for (size_t k = 0; k <= l; k++) { free(A[k]); free(D[k]); }
            free(A); free(D);
            return -4;
        }
    }

    uint8_t *quiet_tmp = NULL;
    const uint8_t *quiet = cfg->quiet_mask;
    if (!quiet && cfg->auto_quiet) {
        quiet_tmp = (uint8_t*)malloc(n);
        if (quiet_tmp && podr_build_auto_quiet_mask(in, n, cfg->quiet_window, cfg->quiet_keep_frac, quiet_tmp) == 0) {
            quiet = quiet_tmp;
        } else {
            free(quiet_tmp);
            quiet_tmp = NULL;
        }
    }

    swt_haar_forward(in, n, L, A, D);

    float sigma = (cfg->sigma_override > 0.0f)
        ? cfg->sigma_override
        : estimate_sigma_from_quiet_details(D[0], quiet, n);
    if (sigma <= 0.0f) sigma = mad_sigma(D[0], n);

    float *thr = NULL;
    if (diag) {
        diag->sigma_est = sigma;
        diag->thr_per_level = (float*)calloc(L, sizeof(float));
        thr = diag->thr_per_level;
    }

    double base = (double)sigma * sqrt(2.0 * log((double)n));

    for (size_t l = 0; l < L; l++) {
        float gain = cfg->level_gain ? cfg->level_gain[l] : 1.0f;
        float t = (float)base * cfg->thresh_scale * gain;
        if (thr) thr[l] = t;

        if (cfg->mode == PODR_THRESH_HARD) {
            for (size_t i = 0; i < n; i++) D[l][i] = hard_thresh(D[l][i], t);
        } else {
            for (size_t i = 0; i < n; i++) D[l][i] = soft_thresh(D[l][i], t);
        }
    }

    swt_haar_inverse(out, n, L, A, D);

    if (quiet_tmp) free(quiet_tmp);
    for (size_t l = 0; l < L; l++) { free(A[l]); free(D[l]); }
    free(A); free(D);

    return 0;
}
