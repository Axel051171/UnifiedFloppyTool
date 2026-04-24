/**
 * @file uft_write_precomp.h
 * @brief Peak-shift compensation for high-density MFM tracks (Mac 800K).
 *
 * @section NAMING
 *
 * The filename says "write_precomp" per MASTER_PLAN.md TA1, but the
 * algorithm is actually POST-compensation: it corrects captured flux
 * transitions that have drifted due to peak-shift effects during
 * recording. Applied after capture, before decoding. Despite the name,
 * it is NOT pre-emphasis for writing. Future rename may be warranted;
 * kept for traceability to the planning doc.
 *
 * @section ATTRIBUTION
 *
 * Algorithm ported from a8rawconv 0.95 by Avery Lee (GPL-2-or-later):
 *   https://github.com/Axel051171/a8rawconv-0.95
 * Specifically a8rawconv/compensation.cpp::postcomp_track_mac800k.
 *
 * Ported to C / UFT conventions. The threshold formula, the `5/12`
 * correction factor, and the min/max clamp to half-distance are
 * verbatim from the reference.
 *
 * Part of MASTER_PLAN.md M2 (TA1). See docs/A8RAWCONV_INTEGRATION_TODO.md.
 */

#ifndef UFT_WRITE_PRECOMP_H
#define UFT_WRITE_PRECOMP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "uft/uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Compensation mode.
 */
typedef enum {
    UFT_PRECOMP_NONE      = 0, /**< No correction */
    UFT_PRECOMP_AUTO      = 1, /**< Auto (currently a no-op, reserved) */
    UFT_PRECOMP_MAC_800K  = 2  /**< Mac 800K peak-shift compensation */
} uft_precomp_mode_t;

/**
 * Apply Mac-800K peak-shift post-compensation to a captured
 * transition array in-place.
 *
 * The algorithm pushes pairs of transitions further apart when they
 * are closer than a track-density-derived threshold. Effect strength
 * scales with how close the pair is: exactly at threshold → no change,
 * half threshold → maximum correction.
 *
 * Behaviour:
 *   - Returns UFT_OK and leaves @p transitions unchanged if @p n < 3.
 *   - Modifies @p transitions[1..n-2] in place; @p transitions[0] and
 *     @p transitions[n-1] are never touched.
 *   - Does not allocate.
 *
 * @param transitions      Array of N transition timestamps (flux
 *                         sample counts relative to index pulse).
 *                         Modified in place.
 * @param n                Number of transitions in the array.
 * @param samples_per_rev  Flux sampling rate × rotation period.
 *                         For a 300 RPM drive sampled at 40 MHz,
 *                         this is 8 000 000.
 * @param phys_track       Physical track number (0-based).
 *                         Inner-track geometry correction is applied
 *                         above track 47 per the reference.
 *
 * @return UFT_OK on success; UFT_ERR_INVALID_ARG if @p transitions
 *         is NULL (even when n < 3 — fail fast on obvious bugs).
 */
uft_error_t uft_precomp_track_mac800k(
    uint32_t *transitions,
    size_t n,
    uint32_t samples_per_rev,
    int phys_track);

/**
 * Dispatch helper: apply compensation according to @p mode.
 * @c UFT_PRECOMP_NONE and @c UFT_PRECOMP_AUTO are currently no-ops.
 */
uft_error_t uft_precomp_apply(
    uint32_t *transitions,
    size_t n,
    uint32_t samples_per_rev,
    int phys_track,
    uft_precomp_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* UFT_WRITE_PRECOMP_H */
