#ifndef OTDR_COMMON_H
#define OTDR_COMMON_H
/**
 * @file otdr_common.h
 * @brief Shared inline utilities for OTDR analysis modules (v2-v12).
 *
 * Eliminates duplicate static helper definitions across:
 *   - otdr_event_core_v2.c   (clampf, cmp_float_asc)
 *   - otdr_align_fuse_v7.c   (cmp_float_asc)
 *   - otdr_event_core_v8.c   (clampf, cmp_float_asc)
 *   - otdr_event_core_v9.c   (cmp_float)
 *   - otdr_event_core_v10.c  (clampf, cmp_float_asc)
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Clamp float to [lo, hi]. */
static inline float otdr_clampf(float x, float lo, float hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

/** qsort comparator: float ascending. */
static inline int otdr_cmp_float_asc(const void *a, const void *b)
{
    float fa = *(const float *)a, fb = *(const float *)b;
    return (fa < fb) ? -1 : (fa > fb);
}

#ifdef __cplusplus
}
#endif
#endif /* OTDR_COMMON_H */
