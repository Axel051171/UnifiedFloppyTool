#include "uft/format/adf.h"
#include <string.h>

uft_rc_t uft_adf_validate_size(uft_adf_geometry_t g, uint64_t len, uft_diag_t *diag)
{
    if (g.cylinders == 0 || g.heads == 0 || g.sectors_per_track == 0 || g.sector_size == 0) {
        uft_diag_set(diag, "adf: invalid geometry");
        return UFT_EINVAL;
    }
    const uint64_t expect = uft_adf_total_bytes(g);
    if (len != expect) {
        uft_diag_set(diag, "adf: size mismatch (geometry vs buffer/file)");
        return UFT_EFORMAT;
    }
    uft_diag_set(diag, "adf: size ok");
    return UFT_OK;
}

uft_rc_t uft_adf_read_from_blockdev(uft_blockdev_t *bd, uft_adf_geometry_t g, uint8_t *out, uft_diag_t *diag)
{
    if (!bd || !out) { uft_diag_set(diag, "adf: invalid args"); return UFT_EINVAL; }
    uft_rc_t rc = uft_blockdev_validate(bd, diag);
    if (rc != UFT_OK) return rc;
    if (bd->sector_size != g.sector_size) { uft_diag_set(diag, "adf: sector size mismatch vs blockdev"); return UFT_EINVAL; }

    const uint64_t total = uft_adf_total_sectors(g);
    for (uint64_t lba = 0; lba < total; lba++) {
        rc = bd->ops.read_lba(bd, lba, 1, out + (size_t)(lba * g.sector_size), diag);
        if (rc != UFT_OK) {
            /* no fake success: stop at first hard error */
            return rc;
        }
    }
    uft_diag_set(diag, "adf: read ok");
    return UFT_OK;
}

uft_rc_t uft_adf_write_to_blockdev(uft_blockdev_t *bd, uft_adf_geometry_t g, const uint8_t *adf, uint64_t adf_len, uft_diag_t *diag)
{
    if (!bd || !adf) { uft_diag_set(diag, "adf: invalid args"); return UFT_EINVAL; }
    uft_rc_t rc = uft_blockdev_validate(bd, diag);
    if (rc != UFT_OK) return rc;
    if (bd->sector_size != g.sector_size) { uft_diag_set(diag, "adf: sector size mismatch vs blockdev"); return UFT_EINVAL; }

    rc = uft_adf_validate_size(g, adf_len, diag);
    if (rc != UFT_OK) return rc;

    const uint64_t total = uft_adf_total_sectors(g);
    for (uint64_t lba = 0; lba < total; lba++) {
        rc = bd->ops.write_lba(bd, lba, 1, adf + (size_t)(lba * g.sector_size), diag);
        if (rc != UFT_OK) return rc;
    }
    if (bd->ops.flush) {
        rc = bd->ops.flush(bd, diag);
        if (rc != UFT_OK) return rc;
    }
    uft_diag_set(diag, "adf: write ok");
    return UFT_OK;
}
