/**
 * @file uft_write_precomp.c
 * @brief Peak-shift compensation for high-density MFM tracks (Mac 800K).
 *
 * Port of a8rawconv 0.95's postcomp_track_mac800k by Avery Lee
 * (GPL-2-or-later). See include/uft/core/uft_write_precomp.h for
 * full attribution.
 */

#include "uft/core/uft_write_precomp.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Clamp helpers (no <algorithm> in C). */
static inline int32_t i32_max(int32_t a, int32_t b) { return a > b ? a : b; }
static inline int32_t i32_min(int32_t a, int32_t b) { return a < b ? a : b; }

uft_error_t uft_precomp_track_mac800k(
    uint32_t *transitions,
    size_t n,
    uint32_t samples_per_rev,
    int phys_track)
{
    if (transitions == NULL) return UFT_ERR_INVALID_ARG;
    if (n < 3) return UFT_OK;   /* Not enough to correct; no-op. */

    /*
     * Threshold: start correcting below roughly 1/45000th of a rotation,
     * with an inner-track geometry factor. Formula verbatim from
     * a8rawconv/compensation.cpp:
     *
     *   int thresh = (int)(0.5 + samples_per_rev / 30000.0 *
     *                       (float)(160 + std::min(track, 47)) / 240.0f);
     */
    int clamped_track = phys_track < 47 ? phys_track : 47;
    float thresh_f = 0.5f
                   + (float)samples_per_rev / 30000.0f
                   * (float)(160 + clamped_track) / 240.0f;
    int32_t thresh = (int32_t)thresh_f;

    uint32_t t0 = transitions[0];
    uint32_t t1 = transitions[1];

    for (size_t i = 2; i < n; ++i) {
        uint32_t t2 = transitions[i];

        int32_t t01 = (int32_t)(t1 - t0);
        int32_t t12 = (int32_t)(t2 - t1);

        int32_t delta1 = i32_max(0, thresh - t01);
        int32_t delta2 = i32_max(0, thresh - t12);

        /*
         * Correction: push t1 by (delta2 - delta1) * 5/12, clamped to
         * at most half of either neighbouring gap (rounded toward zero).
         * This keeps transitions in order (never crossing).
         */
        int32_t shift = ((delta2 - delta1) * 5) / 12;
        int32_t lo = -t01 / 2;
        int32_t hi =  t12 / 2;
        if (shift < lo) shift = lo;
        if (shift > hi) shift = hi;

        transitions[i - 1] = (uint32_t)((int32_t)t1 + shift);

        t0 = t1;
        t1 = t2;
    }

    return UFT_OK;
}

uft_error_t uft_precomp_apply(
    uint32_t *transitions,
    size_t n,
    uint32_t samples_per_rev,
    int phys_track,
    uft_precomp_mode_t mode)
{
    if (transitions == NULL) return UFT_ERR_INVALID_ARG;

    switch (mode) {
    case UFT_PRECOMP_NONE:
    case UFT_PRECOMP_AUTO:   /* Reserved: a8rawconv also returns early. */
        return UFT_OK;

    case UFT_PRECOMP_MAC_800K:
        return uft_precomp_track_mac800k(transitions, n,
                                          samples_per_rev, phys_track);

    default:
        return UFT_ERR_INVALID_ARG;
    }
}
