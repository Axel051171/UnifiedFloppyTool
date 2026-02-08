#ifndef PHI_OTDR_DENOISE_1D_H
#define PHI_OTDR_DENOISE_1D_H

/*
 * phi-OTDR-inspired adaptive denoising (1D) — C99
 * ------------------------------------------------
 * The linked paper denoises φ-OTDR using curvelet transform + adaptive thresholding.
 * True curvelet is a 2D FFT-based transform. For a single 1D trace (flux-analog),
 * the practical analogue is:
 *   - multi-scale transform (stationary wavelet / à trous)
 *   - robust noise estimation from quiet sections
 *   - adaptive thresholding + shrinkage
 *
 * This library implements:
 *   1) Stationary wavelet transform (SWT) with Haar filters (no decimation)
 *   2) Robust sigma estimation via MAD on detail coefficients (quiet sections)
 *   3) Adaptive per-level thresholds and soft/hard thresholding
 *   4) Inverse SWT reconstruction
 *   5) Optional automatic quiet-region detection (low-variance windows)
 *
 * Build:
 *   cc -O2 -std=c99 -Wall -Wextra -pedantic phi_otdr_denoise_1d.c example_main.c -lm
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PODR_THRESH_SOFT = 0,
    PODR_THRESH_HARD = 1
} podr_thresh_mode;

typedef struct podr_config {
    size_t levels;
    podr_thresh_mode mode;
    float thresh_scale;
    const float *level_gain;

    const uint8_t *quiet_mask;

    int auto_quiet;
    size_t quiet_window;
    float quiet_keep_frac;

    float sigma_override;
} podr_config;

typedef struct podr_diag {
    float sigma_est;
    float *thr_per_level; /* allocated if diag != NULL */
} podr_diag;

podr_config podr_default_config(void);

int podr_denoise_1d(const float *in, float *out, size_t n,
                    const podr_config *cfg, podr_diag *diag);

void podr_free_diag(podr_diag *d);

int podr_build_auto_quiet_mask(const float *x, size_t n,
                               size_t window, float keep_frac,
                               uint8_t *quiet_mask_out);

#ifdef __cplusplus
}
#endif

#endif /* PHI_OTDR_DENOISE_1D_H */
