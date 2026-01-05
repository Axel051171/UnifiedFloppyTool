#pragma once
#include "flux_format/flux_format.h"

/*
 * CPC DSK / EDSK (Amstrad CPC) — sector container.
 *
 * Phase-1 target: deterministic READ/WRITE using ufm_disk_t.logical.
 *
 * Parameter surface (CLI will expose later):
 *   - cyls, heads: geometry (fallback: infer from logical sector map)
 *   - spt: sectors per track (0 means "infer")
 *   - sector_size: bytes per sector (0 means "infer")
 *   - gap3: DSK track header GAP3 byte (default 0x4e)
 *   - filler: default fill byte for missing sectors (default 0xe5)
 *   - extended: force EDSK output (default true)
 */

typedef struct fluxfmt_dsk_params {
    uint16_t cyls;
    uint16_t heads;
    uint16_t spt;
    uint32_t sector_size;
    uint8_t  gap3;
    uint8_t  filler;
    bool     extended;
} fluxfmt_dsk_params_t;

/* Fill with deterministic defaults. */
void fluxfmt_dsk_default_params(fluxfmt_dsk_params_t *p);

/* Export a preservation-friendly metadata sidecar for a CPC DSK/EDSK image.
 *
 * Writes JSON (UTF-8) to the provided stream.
 * Content is derived from ufm_disk_t.logical and container-level hints.
 */
int fluxfmt_dsk_export_meta_json(FILE *out, const ufm_disk_t *disk);

#ifdef __cplusplus
extern "C" {
#endif

extern const fluxfmt_plugin_t fluxfmt_dsk_plugin;

#ifdef __cplusplus
}
#endif
