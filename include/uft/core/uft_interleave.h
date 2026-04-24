/**
 * @file uft_interleave.h
 * @brief Atari 8-bit sector interleave calculator.
 *
 * @section ATTRIBUTION
 *
 * Algorithm ported from a8rawconv 0.95 by Avery Lee (GPL-2-or-later):
 *   https://github.com/Axel051171/a8rawconv-0.95
 * Specifically a8rawconv/interleave.cpp::compute_interleave.
 * Ported to C / UFT conventions; the timing math and interleave
 * formulae are verbatim.
 *
 * Part of MASTER_PLAN.md M2 (TA2). See docs/A8RAWCONV_INTEGRATION_TODO.md.
 */

#ifndef UFT_INTERLEAVE_H
#define UFT_INTERLEAVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "uft/uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Interleave-calculation mode.
 */
typedef enum {
    UFT_INTERLEAVE_AUTO         = 0, /**< Auto-pick interleave by size */
    UFT_INTERLEAVE_FORCE_AUTO   = 1, /**< Same as AUTO but overrides existing positions */
    UFT_INTERLEAVE_NONE         = 2, /**< 1:1 sequential layout (no skipping) */
    UFT_INTERLEAVE_XF551_DD_HS  = 3  /**< XF551 high-speed DD (9:1 like SD) */
} uft_interleave_mode_t;

/**
 * Compute a per-sector timing array for a single track.
 *
 * @param timings_out    Out: caller-provided array of @p sector_count floats.
 *                       Each entry is a fractional track position in [0, 1).
 * @param sector_count   Number of sectors on this track (1..256).
 * @param sector_size    Bytes per sector (128, 256, 512, or other).
 * @param mfm            True if MFM-encoded. Currently unused by the
 *                       algorithm but kept for API parity with a8rawconv.
 * @param track          Physical track number (used for head-to-track skew).
 * @param side           Physical side (0 or 1). Reserved; not used currently.
 * @param mode           Interleave mode; see @ref uft_interleave_mode_t.
 *
 * @return UFT_OK on success,
 *         UFT_ERR_INVALID_ARG if @p timings_out is NULL or
 *         @p sector_count is 0 or > 256.
 *
 * The algorithm chooses an interleave factor based on @p sector_size:
 *   - 128-byte sectors (SD 18-spt or ED 26-spt): ≈ N/2 skip
 *   - 256-byte sectors (DD 18-spt):              15/18 × N skip
 *   - 512+ byte sectors:                          1:1 (no skew applied)
 * Then lays the sectors out greedily, starting from a track-dependent offset.
 */
uft_error_t uft_compute_interleave(
    float *timings_out,
    size_t sector_count,
    uint16_t sector_size,
    bool mfm,
    int track,
    int side,
    uft_interleave_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* UFT_INTERLEAVE_H */
