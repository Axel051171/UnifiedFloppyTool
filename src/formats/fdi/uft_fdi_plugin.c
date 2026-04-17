/**
 * @file uft_fdi_plugin.c
 * @brief FDI (Formatted Disk Image) Plugin-B
 *
 * FDI: Header with "FDI" signature, geometry, track table, sector data.
 * Header: sig(3) + wp(1) + cyls(2LE) + heads(2LE) + desc_off(2LE)
 *       + data_off(2LE) + extra_off(2LE) = 14 bytes
 * Track table: (cyls * heads) entries of 4 bytes each:
 *   offset(4LE) relative to data_off
 * Each track: nsec(1) per sector: C(1)+H(1)+R(1)+N(1)+flags(1)+data(sec_size)
 */
#include "uft/uft_format_common.h"

#define FDI_SIG     "FDI"
#define FDI_HDR     14

typedef struct {
    uint8_t *data;
    size_t   size;
    uint16_t cyls;
    uint16_t heads;
    uint16_t data_off;
} fdi_pd_t;

static bool fdi_plugin_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)fs;
    if (s < FDI_HDR) return false;
    if (memcmp(d, FDI_SIG, 3) == 0) {
        uint16_t cyls = uft_read_le16(d + 4);
        uint16_t heads = uft_read_le16(d + 6);
        if (cyls > 0 && cyls <= 96 && heads > 0 && heads <= 2) {
            *c = 92;
        } else {
            *c = 75;
        }
        return true;
    }
    return false;
}

static uft_error_t fdi_plugin_open(uft_disk_t *disk, const char *path, bool ro) {
    (void)ro;
    size_t file_size = 0;
    uint8_t *data = uft_read_file(path, &file_size);
    if (!data || file_size < FDI_HDR) { free(data); return UFT_ERROR_FILE_OPEN; }
    if (memcmp(data, FDI_SIG, 3) != 0) { free(data); return UFT_ERROR_FORMAT_INVALID; }

    fdi_pd_t *p = calloc(1, sizeof(fdi_pd_t));
    if (!p) { free(data); return UFT_ERROR_NO_MEMORY; }
    p->data = data;
    p->size = file_size;
    p->cyls = uft_read_le16(data + 4);
    p->heads = uft_read_le16(data + 6);
    p->data_off = uft_read_le16(data + 10);

    if (p->cyls == 0 || p->cyls > 96 || p->heads == 0 || p->heads > 2) {
        free(data); free(p); return UFT_ERROR_FORMAT_INVALID;
    }

    /* Scan first track for sector size and count */
    uint16_t sec_size = 512;
    uint8_t  spt = 9;
    size_t trk_table_off = FDI_HDR;
    if (trk_table_off + 4 <= file_size) {
        uint32_t first_trk_off = uft_read_le32(data + trk_table_off);
        size_t abs_off = (size_t)p->data_off + first_trk_off;
        if (abs_off < file_size) {
            spt = data[abs_off];
            if (spt > 0 && abs_off + 1 + 6 <= file_size) {
                uint8_t n = data[abs_off + 1 + 3]; /* N field */
                if (n <= 6) sec_size = (uint16_t)(128 << n);
            }
        }
    }

    disk->plugin_data = p;
    disk->geometry.cylinders = p->cyls;
    disk->geometry.heads = p->heads;
    disk->geometry.sectors = spt;
    disk->geometry.sector_size = sec_size;
    disk->geometry.total_sectors = (uint32_t)p->cyls * p->heads * spt;
    return UFT_OK;
}

static void fdi_plugin_close(uft_disk_t *disk) {
    fdi_pd_t *p = disk->plugin_data;
    if (p) { free(p->data); free(p); disk->plugin_data = NULL; }
}

static uft_error_t fdi_plugin_read_track(uft_disk_t *disk, int cyl, int head,
                                          uft_track_t *track) {
    fdi_pd_t *p = disk->plugin_data;
    if (!p || !p->data) return UFT_ERROR_INVALID_STATE;
    if (cyl >= p->cyls || head >= p->heads) return UFT_ERROR_INVALID_ARG;

    uft_track_init(track, cyl, head);

    /* Track table: entry at FDI_HDR + (cyl * heads + head) * 4 */
    size_t entry_off = FDI_HDR + ((size_t)cyl * p->heads + head) * 4;
    if (entry_off + 4 > p->size) return UFT_OK;

    uint32_t trk_off = uft_read_le32(p->data + entry_off);
    size_t abs_off = (size_t)p->data_off + trk_off;
    if (abs_off >= p->size) return UFT_OK;

    uint8_t nsec = p->data[abs_off];
    size_t pos = abs_off + 1;

    for (int s = 0; s < nsec && pos + 5 <= p->size; s++) {
        uint8_t sec_c = p->data[pos];
        uint8_t sec_h = p->data[pos + 1];
        uint8_t sec_r = p->data[pos + 2];
        uint8_t sec_n = p->data[pos + 3];
        uint8_t flags = p->data[pos + 4];
        pos += 5;

        (void)sec_c; (void)sec_h; (void)flags;
        uint16_t ss = (sec_n <= 6) ? (uint16_t)(128 << sec_n) : 512;

        if (pos + ss > p->size) break;
        uft_format_add_sector(track, sec_r > 0 ? sec_r - 1 : 0,
                              p->data + pos, ss,
                              (uint8_t)cyl, (uint8_t)head);
        pos += ss;
    }
    return UFT_OK;
}

static uft_error_t fdi_plugin_write_track(uft_disk_t *disk, int cyl, int head,
                                            const uft_track_t *track) {
    fdi_pd_t *p = disk->plugin_data;
    if (!p || !p->data) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    if (cyl >= p->cyls || head >= p->heads) return UFT_ERROR_INVALID_ARG;

    /* Track table: entry at FDI_HDR + (cyl * heads + head) * 4 */
    size_t entry_off = FDI_HDR + ((size_t)cyl * p->heads + head) * 4;
    if (entry_off + 4 > p->size) return UFT_ERROR_INVALID_ARG;

    uint32_t trk_off = uft_read_le32(p->data + entry_off);
    size_t abs_off = (size_t)p->data_off + trk_off;
    if (abs_off >= p->size) return UFT_ERROR_INVALID_ARG;

    uint8_t nsec = p->data[abs_off];
    size_t pos = abs_off + 1;

    for (int s = 0; s < nsec && pos + 5 <= p->size; s++) {
        uint8_t sec_r = p->data[pos + 2];
        uint8_t sec_n = p->data[pos + 3];
        pos += 5;

        uint16_t ss = (sec_n <= 6) ? (uint16_t)(128 << sec_n) : 512;
        if (pos + ss > p->size) break;

        /* Find matching sector in input track by R field */
        uint8_t match_id = sec_r > 0 ? sec_r - 1 : 0;
        for (size_t ts = 0; ts < track->sector_count; ts++) {
            if (track->sectors[ts].id.sector == match_id) {
                const uint8_t *src = track->sectors[ts].data;
                if (src && track->sectors[ts].data_len >= ss)
                    memcpy(p->data + pos, src, ss);
                break;
            }
        }
        pos += ss;
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_fdi = {
    .name = "FDI", .description = "Formatted Disk Image",
    .extensions = "fdi", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = fdi_plugin_probe, .open = fdi_plugin_open,
    .close = fdi_plugin_close, .read_track = fdi_plugin_read_track,
    .write_track = fdi_plugin_write_track,
    .verify_track = uft_generic_verify_track,
};
UFT_REGISTER_FORMAT_PLUGIN(fdi)
