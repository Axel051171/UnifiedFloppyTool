/**
 * @file uft_interleave.c
 * @brief Atari 8-bit sector interleave calculator.
 *
 * Port of a8rawconv 0.95's compute_interleave() by Avery Lee
 * (GPL-2-or-later), adapted to C and UFT error conventions.
 * See include/uft/core/uft_interleave.h for full attribution.
 */

#include "uft/core/uft_interleave.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define UFT_INTERLEAVE_MAX_SECTORS 256u

uft_error_t uft_compute_interleave(
    float *timings_out,
    size_t sector_count,
    uint16_t sector_size,
    bool mfm,
    int track,
    int side,
    uft_interleave_mode_t mode)
{
    (void)mfm;   /* reserved for future mfm-specific tuning */
    (void)side;

    if (timings_out == NULL) return UFT_ERR_INVALID_ARG;
    if (sector_count == 0) return UFT_ERR_INVALID_ARG;
    if (sector_count > UFT_INTERLEAVE_MAX_SECTORS) return UFT_ERR_INVALID_ARG;

    /* Initialise output to -1 to mirror a8rawconv's "unset" marker. */
    for (size_t i = 0; i < sector_count; ++i) timings_out[i] = -1.0f;

    /* Assume 8% track-to-track skew (~16.7 ms at 300 RPM). */
    float t0 = 0.08f * (float)track;
    const float spacing = 0.98f / (float)sector_count;

    /* Interleave factor selection (verbatim from a8rawconv). */
    int interleave = 1;
    switch (mode) {
    case UFT_INTERLEAVE_AUTO:
    case UFT_INTERLEAVE_FORCE_AUTO:
        if (sector_size == 128u) {
            interleave = (int)((sector_count + 1u) / 2u);
        } else if (sector_size == 256u) {
            interleave = (int)((sector_count * 15u + 17u) / 18u);
        } else {
            /* For 512+ byte sectors: 1:1 and no skew. */
            t0 = 0.0f;
        }
        break;

    case UFT_INTERLEAVE_NONE:
        interleave = 1;
        t0 = 0.0f;
        break;

    case UFT_INTERLEAVE_XF551_DD_HS:
        /* 9:1-style interleave for 18 DD sectors. */
        interleave = (int)((sector_count + 1u) / 2u);
        break;

    default:
        return UFT_ERR_INVALID_ARG;
    }

    /* Greedy slot allocation. Starts at slot 0 and advances by
     * `interleave` each iteration, skipping occupied slots. */
    bool occupied[UFT_INTERLEAVE_MAX_SECTORS];
    memset(occupied, 0, sizeof(occupied));

    int slot_idx = 0;
    for (size_t i = 0; i < sector_count; ++i) {
        while (occupied[slot_idx]) {
            if (++slot_idx >= (int)sector_count) slot_idx = 0;
        }
        occupied[slot_idx] = true;

        float t = t0 + spacing * (float)slot_idx;
        timings_out[i] = t - floorf(t);

        slot_idx += interleave;
        if (slot_idx >= (int)sector_count) slot_idx -= (int)sector_count;
    }

    return UFT_OK;
}
