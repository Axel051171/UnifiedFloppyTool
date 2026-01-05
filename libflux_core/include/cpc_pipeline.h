#pragma once
/*
 * cpc_pipeline.h — CPC: Flux -> (PLL) raw MFM -> sector decode
 *
 * This is the "glue" layer that demonstrates:
 *   - Index-Pulse-Slicing (track windowing)
 *   - PLL bitcell reconstruction
 *   - CPC/IBM MFM sector parsing
 */

#include <stdint.h>

#include "flux_core.h"
#include "flux_decode.h"
#include "flux_window.h"
#include "flux_consensus.h"
#include "cpc_mfm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cpc_flux_decode_result {
    uint32_t slices_tried;
    uint32_t slices_used;
    flux_pll_stats_t pll_stats;
    flux_consensus_stats_t consensus_stats;
    cpc_mfm_decode_stats_t mfm_stats;
} cpc_flux_decode_result_t;

/* Decode a captured track revolution into sectors.
 *
 * Strategy:
 *  - Slice by index pulses (window per revolution)
 *  - Run PLL on each slice
 *  - Run sector decoder
 *  - Keep the slice with the best "quality" (most sectors, fewest CRC issues)
 */
int cpc_decode_track_from_ufm(
    const ufm_revolution_t *cap,
    uint16_t cyl, uint16_t head,
    const flux_pll_params_t *pll,
    ufm_logical_image_t *li_out,
    cpc_flux_decode_result_t *res_out);

#ifdef __cplusplus
}
#endif
