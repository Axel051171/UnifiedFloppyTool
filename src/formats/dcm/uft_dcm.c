/**
 * @file uft_dcm.c
 * @brief DCM (DiskComm Compressed Atari) Plugin
 *
 * DCM is a compressed Atari disk format by Bob Puff (DiskComm).
 * Header byte 0xF9 or 0xFA, followed by pass data with RLE compression.
 *
 * Density: 0=SD(128), 1=ED(128), 2=DD(256), 3=DD(256)
 * This plugin reads the header/metadata. Full decompression would
 * require implementing DCM's 5 compression methods.
 *
 * Reference: SIO2PC documentation, Atari800 DCM loader source
 */
#include "uft/uft_format_common.h"

#define DCM_MAGIC     0xF9
#define DCM_MAGIC_ALT 0xFA

typedef struct {
    uint8_t*    file_data;
    size_t      file_size;
    uint8_t     density;    /* 0=SD, 1=ED, 2=DD */
    uint16_t    sector_size;
    uint32_t    total_sectors;
} dcm_data_t;

bool dcm_probe(const uint8_t *data, size_t size, size_t file_size, int *confidence) {
    (void)file_size;
    if (size < 5) return false;
    if (data[0] == DCM_MAGIC || data[0] == DCM_MAGIC_ALT) {
        *confidence = 85;
        return true;
    }
    return false;
}

static uft_error_t dcm_open(uft_disk_t *disk, const char *path, bool ro) {
    (void)ro;
    size_t file_size = 0;
    uint8_t *data = uft_read_file(path, &file_size);
    if (!data || file_size < 5) { free(data); return UFT_ERROR_FORMAT_INVALID; }

    if (data[0] != DCM_MAGIC && data[0] != DCM_MAGIC_ALT) {
        free(data); return UFT_ERROR_FORMAT_INVALID;
    }

    dcm_data_t *p = calloc(1, sizeof(dcm_data_t));
    if (!p) { free(data); return UFT_ERROR_NO_MEMORY; }

    p->file_data = data;
    p->file_size = file_size;
    p->density = (data[1] >> 2) & 0x03;
    p->sector_size = (p->density <= 1) ? 128 : 256;
    p->total_sectors = 720;  /* Standard Atari: 40 tracks × 18 sectors */

    disk->plugin_data = p;
    disk->geometry.cylinders = 40;
    disk->geometry.heads = 1;
    disk->geometry.sectors = 18;
    disk->geometry.sector_size = p->sector_size;
    disk->geometry.total_sectors = p->total_sectors;
    return UFT_OK;
}

static void dcm_close(uft_disk_t *d) {
    dcm_data_t *p = d->plugin_data;
    if (p) { free(p->file_data); free(p); d->plugin_data = NULL; }
}

static uft_error_t dcm_read_track(uft_disk_t *d, int cyl, int head, uft_track_t *t) {
    dcm_data_t *p = d->plugin_data;
    if (!p || head != 0) return UFT_ERROR_INVALID_STATE;
    uft_track_init(t, cyl, head);
    /* DCM decompression not yet implemented — return empty sectors
     * with status indicating compressed source */
    for (int s = 0; s < 18; s++) {
        uft_format_add_empty_sector(t, (uint8_t)s, p->sector_size,
                                     0xE5, (uint8_t)cyl, 0);
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_dcm = {
    .name = "DCM", .description = "DiskComm Compressed Atari",
    .extensions = "dcm", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ,
    .probe = dcm_probe, .open = dcm_open, .close = dcm_close,
    .read_track = dcm_read_track,
};
UFT_REGISTER_FORMAT_PLUGIN(dcm)
