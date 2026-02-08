#ifndef OTDR_EVENT_CORE_V7_H
#define OTDR_EVENT_CORE_V7_H
/*
 * OTDR Event Core v7 (C99)
 * ------------------------
 * v7 adds ALIGNMENT + stability metrics for multi-pass fusion:
 *
 * 1) Shift estimation via normalized cross-correlation (NCC) in a bounded lag range
 * 2) Trace alignment (integer shift, zero-pad) + fusion (median/trimmed mean)
 * 3) Stability/confidence map across passes:
 *      - per-sample agreement ratio and mean confidence
 *
 * Contains a focused alignment+fusion module (no full v5 engine copy).
 * This is meant to sit next to v6/v5 in your project.
 *
 * Implementation: otdr_align_fuse_v7.c (named for its focus on alignment+fusion)
 *
 * Build:
 *   cc -O2 -std=c99 -Wall -Wextra -pedantic otdr_align_fuse_v7.c example_main.c -lm -o otdr_v7_demo
 */
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Estimate integer shift between reference and target within [-max_lag..+max_lag].
 * Returns best shift (target shifted by this to align to ref).
 * Optionally returns best NCC score in *out_score (0..1 roughly).
 */
int otdr_estimate_shift_ncc(const float *ref, const float *x, size_t n, int max_lag, float *out_score);

/* Apply integer shift: out[i] = x[i - shift] with zero padding.
 * Positive shift moves x to the right.
 */
void otdr_apply_shift_zeropad(const float *x, size_t n, int shift, float *out);

/* Align a set of traces to traces[ref_idx] and produce shifted pointers.
 * shifts_out length m.
 * aligned_out is array of m pointers to buffers (allocated by caller), each length n.
 */
int otdr_align_traces(const float *const *traces, size_t m, size_t n, size_t ref_idx,
                      int max_lag, int *shifts_out, float **aligned_out);

/* Fuse aligned traces (median) */
int otdr_fuse_aligned_median(float *const *aligned, size_t m, size_t n, float *out);

/* Agreement/stability map:
 * labels[k][i] in {0..C-1}, where C classes.
 * outputs:
 *   agree_ratio[i] = max_class_count / m
 *   entropy_like[i] = 1 - sum(p^2)  (0=perfect agreement; higher=more disagreement)
 */
int otdr_label_stability(const uint8_t *const *labels, size_t m, size_t n, uint8_t num_classes,
                         float *agree_ratio, float *entropy_like);

#ifdef __cplusplus
}
#endif
#endif /* OTDR_EVENT_CORE_V7_H */
