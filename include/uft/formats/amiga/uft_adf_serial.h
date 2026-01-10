#pragma once
/*
 * adf.h — Amiga Disk File (ADF) container helpers.
 *
 * Context extracted from TransWarp/Transdisk tools:
 * - raw 512-byte sectors written sequentially by track/sector
 * - standard DD ADF: 80 cylinders, 2 heads, 11 sectors/track -> 901,120 bytes
 *
 * This module is container-level: it does not decode MFM; it handles the common raw-sector ADF layout.
 */
#include <stddef.h>
#include <stdint.h>
#include "uft/uft_common.h"
#include "uft/hal/blockdev.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uft_adf_geometry {
    uint32_t cylinders;         /* usually 80 */
    uint32_t heads;             /* usually 2 */
    uint32_t sectors_per_track; /* usually 11 (DD), 22 (HD) */
    uint32_t sector_size;       /* bytes, usually 512 */
} uft_adf_geometry_t;

static inline uft_adf_geometry_t uft_adf_geometry_dd(void)
{
    uft_adf_geometry_t g = {80u, 2u, 11u, 512u};
    return g;
}

static inline uint64_t uft_adf_total_sectors(uft_adf_geometry_t g)
{
    return (uint64_t)g.cylinders * (uint64_t)g.heads * (uint64_t)g.sectors_per_track;
}

static inline uint64_t uft_adf_total_bytes(uft_adf_geometry_t g)
{
    return uft_adf_total_sectors(g) * (uint64_t)g.sector_size;
}

/* Validates that `len` matches the geometry exactly. */
uft_rc_t uft_adf_validate_size(uft_adf_geometry_t g, uint64_t len, uft_diag_t *diag);

/*
 * Reads a whole disk via blockdev into an ADF buffer.
 * Ownership: caller provides `out` buffer of size uft_adf_total_bytes(g).
 */
uft_rc_t uft_adf_read_from_blockdev(uft_blockdev_t *bd, uft_adf_geometry_t g, uint8_t *out, uft_diag_t *diag);

/*
 * Writes an ADF buffer to a blockdev.
 * Safety: validates size and uses flush if available.
 */
uft_rc_t uft_adf_write_to_blockdev(uft_blockdev_t *bd, uft_adf_geometry_t g, const uint8_t *adf, uint64_t adf_len, uft_diag_t *diag);

#ifdef __cplusplus
}
#endif
