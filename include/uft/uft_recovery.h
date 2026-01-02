/**
 * @file uft_recovery.h
 * @brief Multi-pass recovery helpers (bit voting, re-alignment).
 */

#ifndef UFT_RECOVERY_H
#define UFT_RECOVERY_H

#include <stddef.h>
#include <stdint.h>

#include "uft/flux_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t mfm_sync;         /**< sync word to align by (e.g. 0x4489 for IBM) */
    uint32_t max_bits;         /**< cap output length */
    uint32_t min_passes;       /**< require at least N passes */
} uft_recovery_cfg_t;

uft_recovery_cfg_t uft_recovery_cfg_default(void);

/**
 * @brief Decode multiple flux reads of the same track and vote a best-effort bitstream.
 *
 * Output is a packed bitstream (MSB-first). Returns bits written.
 * out_quality is a 0..1 estimate based on unanimity and decode drop rate.
 */
size_t uft_recover_mfm_track_multipass(
    const flux_track_t *const *passes,
    size_t pass_count,
    const uft_recovery_cfg_t *cfg,
    uint8_t *out_bits,
    size_t out_capacity_bits,
    float *out_quality
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_RECOVERY_H */
