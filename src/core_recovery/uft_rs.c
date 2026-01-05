#include "uft/uft_rs.h"

#include <string.h>

/* GF(256) with primitive poly 0x11d */
static uint8_t gf_exp[512];
static uint8_t gf_log[256];
static int gf_inited = 0;

static void gf_init_tables(void)
{
    if (gf_inited) return;
    uint16_t x = 1;
    for (int i = 0; i < 255; ++i) {
        gf_exp[i] = (uint8_t)x;
        gf_log[x] = (uint8_t)i;
        x <<= 1;
        if (x & 0x100) x ^= 0x11d;
    }
    for (int i = 255; i < 512; ++i) gf_exp[i] = gf_exp[i - 255];
    gf_log[0] = 0; /* undefined, but keep 0 */
    gf_inited = 1;
}

static inline uint8_t gf_mul(uint8_t a, uint8_t b)
{
    if (a == 0 || b == 0) return 0;
    return gf_exp[gf_log[a] + gf_log[b]];
}

static inline uint8_t gf_div(uint8_t a, uint8_t b)
{
    if (a == 0) return 0;
    if (b == 0) return 0; /* invalid */
    int diff = (int)gf_log[a] - (int)gf_log[b];
    if (diff < 0) diff += 255;
    return gf_exp[diff];
}

static inline uint8_t gf_pow(uint8_t a, int p)
{
    if (p == 0) return 1;
    if (a == 0) return 0;
    int r = (int)gf_log[a] * p;
    while (r < 0) r += 255;
    r %= 255;
    return gf_exp[r];
}

static void poly_scale_add(uint8_t *dst, const uint8_t *src, int len, uint8_t scale)
{
    for (int i = 0; i < len; ++i) dst[i] ^= gf_mul(src[i], scale);
}

/* Evaluate polynomial (descending powers) at x */
static uint8_t poly_eval(const uint8_t *p, int plen, uint8_t x)
{
    uint8_t y = p[0];
    for (int i = 1; i < plen; ++i) y = gf_mul(y, x) ^ p[i];
    return y;
}

int uft_rs_init(uft_rs_t *rs, int nsyms)
{
    if (!rs) return 0;
    if (nsyms < 2 || nsyms > 128) return 0;
    gf_init_tables();
    rs->nsyms = nsyms;
    return 1;
}

/* Berlekamp-Massey: compute error locator polynomial sigma */
static int berlekamp_massey(const uint8_t *synd, int nsym, uint8_t *sigma, int *sigma_len)
{
    /* sigma and B polynomials (max nsym+1) */
    uint8_t C[256] = {0};
    uint8_t B[256] = {0};
    C[0] = 1;
    B[0] = 1;
    int L = 0;
    int m = 1;
    uint8_t b = 1;

    for (int n = 0; n < nsym; ++n) {
        /* discrepancy d = synd[n] + sum_{i=1..L} C[i]*synd[n-i] */
        uint8_t d = synd[n];
        for (int i = 1; i <= L; ++i) d ^= gf_mul(C[i], synd[n - i]);

        if (d == 0) {
            m++;
            continue;
        }

        uint8_t coef = gf_div(d, b);
        uint8_t T[256];
        memcpy(T, C, sizeof(T));

        /* C(x) = C(x) - coef*x^m*B(x)  (over GF, - is +) */
        for (int i = 0; i < 256 - m; ++i) {
            if (B[i] == 0) continue;
            C[i + m] ^= gf_mul(coef, B[i]);
        }

        if (2 * L <= n) {
            L = n + 1 - L;
            memcpy(B, T, sizeof(B));
            b = d;
            m = 1;
        } else {
            m++;
        }
    }

    int len = L + 1;
    memcpy(sigma, C, (size_t)len);
    *sigma_len = len;
    return L;
}

/* Compute error evaluator polynomial omega = (synd * sigma) mod x^nsym */
static void compute_omega(const uint8_t *synd, int nsym, const uint8_t *sigma, int sigma_len, uint8_t *omega, int *omega_len)
{
    uint8_t tmp[512] = {0};
    /* convolution: synd (degree nsym-1) with sigma */
    for (int i = 0; i < nsym; ++i) {
        for (int j = 0; j < sigma_len; ++j) {
            tmp[i + j] ^= gf_mul(synd[i], sigma[j]);
        }
    }
    /* mod x^nsym => keep first nsym coefficients */
    memcpy(omega, tmp, (size_t)nsym);
    *omega_len = nsym;
}

/* formal derivative of sigma (forney denominator), sigma is in ascending order */
static void sigma_derivative(const uint8_t *sigma, int sigma_len, uint8_t *ds, int *ds_len)
{
    int out = 0;
    for (int i = 1; i < sigma_len; ++i) {
        if (i & 1) {
            ds[out++] = sigma[i];
        }
    }
    if (out == 0) {
        ds[0] = 0;
        out = 1;
    }
    *ds_len = out;
}

int uft_rs_decode(uft_rs_t *rs, uint8_t *msg, size_t len)
{
    if (!rs || !msg || len == 0) return -1;
    gf_init_tables();

    const int nsym = rs->nsyms;
    if ((int)len <= nsym) return -1;

    /* compute syndromes S_i = r(alpha^i), i=1..nsym */
    uint8_t synd[256] = {0};
    int has_err = 0;
    for (int i = 0; i < nsym; ++i) {
        uint8_t x = gf_pow(2, i); /* alpha^i where alpha=2 */
        uint8_t s = 0;
        for (size_t j = 0; j < len; ++j) {
            s = gf_mul(s, x) ^ msg[j];
        }
        synd[i] = s;
        if (s) has_err = 1;
    }
    if (!has_err) return 0;

    /* Find sigma (error locator) */
    uint8_t sigma[256] = {0};
    int sigma_len = 0;
    int L = berlekamp_massey(synd, nsym, sigma, &sigma_len);
    if (L <= 0) return -1;

    /* Find error locations via Chien search */
    int err_pos[256];
    int err_count = 0;
    for (size_t i = 0; i < len; ++i) {
        /* Evaluate sigma at alpha^{-i} */
        uint8_t x = gf_exp[255 - ((int)i % 255)]; /* alpha^{-i} */
        uint8_t y = poly_eval(sigma, sigma_len, x);
        if (y == 0) {
            err_pos[err_count++] = (int)(len - 1 - i);
            if (err_count > L) break;
        }
    }
    if (err_count != L) return -1;

    /* omega */
    uint8_t omega[256] = {0};
    int omega_len = 0;
    compute_omega(synd, nsym, sigma, sigma_len, omega, &omega_len);

    /* sigma' */
    uint8_t ds[256] = {0};
    int ds_len = 0;
    sigma_derivative(sigma, sigma_len, ds, &ds_len);

    /* Forney: correct each error */
    for (int e = 0; e < err_count; ++e) {
        int p = err_pos[e];
        /* x = alpha^{-(len-1-p)} */
        int i = (int)(len - 1 - (size_t)p);
        uint8_t x = gf_exp[255 - (i % 255)];
        uint8_t x_inv = gf_div(1, x);

        /* magnitude = omega(x) / sigma'(x) */
        uint8_t y = poly_eval(omega, omega_len, x);
        uint8_t z = poly_eval(ds, ds_len, x);
        if (z == 0) return -1;

        uint8_t mag = gf_div(y, z);
        /* apply at position */
        msg[p] ^= mag;
        (void)x_inv;
    }

    return err_count;
}
