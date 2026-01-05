#pragma once
/*
 * cpc_meta.h — CPC MFM decode metadata export
 *
 * Meta export is *lossless* regarding what our decoder observed:
 *  - IDAM fields (C/H/R/N), DAM type (normal/deleted)
 *  - CRC read vs computed (IDAM + DAM)
 *  - Bit positions (correlate back to raw MFM / flux)
 *
 * Output format: JSON (human + machine readable).
 */

#include <stdint.h>
#include <stdio.h>

#include "flux_logical.h"
#include "cpc_pipeline.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Write a JSON description of the logical sectors currently in `li`.
 *
 * Notes:
 *  - Only sectors with meta_type==UFM_SECTOR_META_MFM_IBM will include CRC/bitpos info.
 *  - This is *not* a DSK writer. It's a forensic sidecar for preservation workflows.
 */
int cpc_write_sector_map_json(
    FILE *out,
    uint16_t cyl, uint16_t head,
    const ufm_logical_image_t *li,
    const cpc_flux_decode_result_t *decode_stats);

#ifdef __cplusplus
}
#endif
