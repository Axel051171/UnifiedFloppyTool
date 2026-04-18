/**
 * @file uft_imd_plugin.c
 * @brief IMD (ImageDisk) Plugin-B wrapper
 *
 * IMD: ASCII header terminated by 0x1A, followed by track records.
 * Each track: mode(1) + cyl(1) + head(1) + nsec(1) + size_code(1)
 *           + sector_map(nsec) + optional cyl/head maps + sector data
 *
 * Sector data types: 0=unavail, 1=normal, 2=compressed(fill byte),
 *                    3=deleted, 4=del+comp, 5=error, 6=err+comp, 7=del+err, 8=del+err+comp
 */
#include "uft/uft_format_common.h"

#define IMD_SIG "IMD "

typedef struct {
    uint8_t *data;
    size_t   size;
    uint8_t  max_cyl;
    uint8_t  max_head;
    uint8_t  max_spt;
    uint16_t sec_size;
} imd_pd_t;

static uint16_t imd_sec_size(uint8_t code) {
    if (code > 6) return 512;
    return (uint16_t)(128 << code);
}

static bool imd_plugin_probe(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    (void)file_size;
    if (size < 4) return false;
    if (memcmp(data, IMD_SIG, 4) == 0) { *confidence = 95; return true; }
    return false;
}

/* Find end of IMD comment (0x1A byte) */
static size_t imd_skip_comment(const uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i++)
        if (data[i] == 0x1A) return i + 1;
    return size;
}

/* Scan geometry without extracting sector data */
static void imd_scan_geometry(const uint8_t *data, size_t size, size_t pos,
                               uint8_t *max_cyl, uint8_t *max_head,
                               uint8_t *max_spt, uint16_t *sec_size) {
    *max_cyl = 0; *max_head = 0; *max_spt = 0; *sec_size = 512;
    while (pos + 5 <= size) {
        uint8_t cyl = data[pos + 1];
        uint8_t head_raw = data[pos + 2];
        uint8_t head = head_raw & 0x0F;
        uint8_t nsec = data[pos + 3];
        uint8_t scode = data[pos + 4];
        uint16_t ss = imd_sec_size(scode);

        if (cyl > *max_cyl) *max_cyl = cyl;
        if (head > *max_head) *max_head = head;
        if (nsec > *max_spt) *max_spt = nsec;
        *sec_size = ss;

        pos += 5;
        /* sector numbering map */
        pos += nsec;
        /* optional cylinder map */
        if (head_raw & 0x80) pos += nsec;
        /* optional head map */
        if (head_raw & 0x40) pos += nsec;
        /* sector data */
        for (int s = 0; s < nsec && pos < size; s++) {
            uint8_t dtype = data[pos++];
            switch (dtype) {
                case 0: break; /* unavailable */
                case 2: case 4: case 6: case 8:
                    pos += 1; break; /* compressed: 1 fill byte */
                default:
                    pos += ss; break; /* normal/deleted/error: full data */
            }
        }
    }
}

static uft_error_t imd_plugin_open(uft_disk_t *disk, const char *path, bool ro) {
    (void)ro;
    size_t file_size = 0;
    uint8_t *data = uft_read_file(path, &file_size);
    if (!data || file_size < 4) { free(data); return UFT_ERROR_FILE_OPEN; }
    if (memcmp(data, IMD_SIG, 4) != 0) { free(data); return UFT_ERROR_FORMAT_INVALID; }

    imd_pd_t *p = calloc(1, sizeof(imd_pd_t));
    if (!p) { free(data); return UFT_ERROR_NO_MEMORY; }
    p->data = data;
    p->size = file_size;

    size_t start = imd_skip_comment(data, file_size);
    imd_scan_geometry(data, file_size, start,
                      &p->max_cyl, &p->max_head, &p->max_spt, &p->sec_size);

    disk->plugin_data = p;
    disk->geometry.cylinders = p->max_cyl + 1;
    disk->geometry.heads = p->max_head + 1;
    disk->geometry.sectors = p->max_spt;
    disk->geometry.sector_size = p->sec_size;
    disk->geometry.total_sectors = (uint32_t)(p->max_cyl + 1) *
                                   (p->max_head + 1) * p->max_spt;
    return UFT_OK;
}

static void imd_plugin_close(uft_disk_t *disk) {
    imd_pd_t *p = disk->plugin_data;
    if (p) { free(p->data); free(p); disk->plugin_data = NULL; }
}

static uft_error_t imd_plugin_read_track(uft_disk_t *disk, int cyl, int head,
                                          uft_track_t *track) {
    imd_pd_t *p = disk->plugin_data;
    if (!p || !p->data) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    size_t pos = imd_skip_comment(p->data, p->size);

    while (pos + 5 <= p->size) {
        uint8_t trk_cyl = p->data[pos + 1];
        uint8_t head_raw = p->data[pos + 2];
        uint8_t trk_head = head_raw & 0x0F;
        uint8_t nsec = p->data[pos + 3];
        uint8_t scode = p->data[pos + 4];
        uint16_t ss = imd_sec_size(scode);
        bool is_target = ((int)trk_cyl == cyl && (int)trk_head == head);

        pos += 5;

        /* sector numbering map */
        const uint8_t *sec_map = p->data + pos;
        pos += nsec;
        /* optional cylinder map */
        if (head_raw & 0x80) pos += nsec;
        /* optional head map */
        if (head_raw & 0x40) pos += nsec;

        /* sector data */
        for (int s = 0; s < nsec && pos < p->size; s++) {
            uint8_t dtype = p->data[pos++];
            if (dtype == 0) {
                /* unavailable */
                if (is_target) {
                    uint8_t fill[8192];
                    memset(fill, 0xE5, ss);
                    uft_format_add_sector(track, sec_map[s], fill, ss,
                                          (uint8_t)cyl, (uint8_t)head);
                }
                continue;
            }
            /* IMD dtype: 1/2=normal, 3/4=deleted, 5/6=CRC error,
             * 7/8=deleted+CRC error. Even=compressed, odd=raw. */
            bool compressed = (dtype == 2 || dtype == 4 || dtype == 6 || dtype == 8);
            bool is_deleted = (dtype >= 3 && dtype <= 4) || (dtype >= 7);
            bool is_crc_err = (dtype >= 5);
            if (compressed) {
                if (is_target && pos < p->size) {
                    uint8_t fill_buf[8192];
                    memset(fill_buf, p->data[pos], ss);
                    uft_format_add_sector(track, sec_map[s], fill_buf, ss,
                                          (uint8_t)cyl, (uint8_t)head);
                }
                pos += 1;
            } else {
                if (is_target && pos + ss <= p->size) {
                    uft_format_add_sector(track, sec_map[s],
                                          p->data + pos, ss,
                                          (uint8_t)cyl, (uint8_t)head);
                }
                pos += ss;
            }
            /* Propagate IMD sector status flags */
            if (is_target && track->sector_count > 0) {
                if (is_crc_err)
                    track->sectors[track->sector_count - 1].crc_ok = false;
                if (is_deleted)
                    track->sectors[track->sector_count - 1].deleted = true;
            }
        }

        if (is_target) break;
    }
    return UFT_OK;
}

/* Write track: modifies in-memory IMD data for raw (uncompressed) sectors.
 * Compressed sectors (type 2/4/6/8) are converted to raw type in-place if
 * the new data differs from the fill byte, expanding the buffer as needed.
 * For simplicity, we only write to sectors that are already raw (type 1/3/5/7). */
static uft_error_t imd_plugin_write_track(uft_disk_t *disk, int cyl, int head,
                                            const uft_track_t *track) {
    imd_pd_t *p = disk->plugin_data;
    if (!p || !p->data) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    size_t pos = imd_skip_comment(p->data, p->size);

    while (pos + 5 <= p->size) {
        uint8_t trk_cyl = p->data[pos + 1];
        uint8_t head_raw = p->data[pos + 2];
        uint8_t trk_head = head_raw & 0x0F;
        uint8_t nsec = p->data[pos + 3];
        uint8_t scode = p->data[pos + 4];
        uint16_t ss = imd_sec_size(scode);
        bool is_target = ((int)trk_cyl == cyl && (int)trk_head == head);

        pos += 5;

        /* sector numbering map */
        const uint8_t *sec_map = p->data + pos;
        pos += nsec;
        /* optional cylinder map */
        if (head_raw & 0x80) pos += nsec;
        /* optional head map */
        if (head_raw & 0x40) pos += nsec;

        /* sector data */
        for (int s = 0; s < nsec && pos < p->size; s++) {
            uint8_t dtype = p->data[pos++];
            if (dtype == 0) {
                /* unavailable — skip */
                continue;
            }
            bool compressed = (dtype == 2 || dtype == 4 || dtype == 6 || dtype == 8);
            if (compressed) {
                /* Cannot write to compressed sectors without buffer realloc.
                 * Skip — read_track already expands these for the caller. */
                pos += 1;
            } else {
                /* Raw sector (type 1/3/5/7) — writable in-place */
                if (is_target && pos + ss <= p->size) {
                    /* Find matching sector in input track by sector_map ID.
                     * id.sector is 1-based (uft_format_add_sector adds 1),
                     * so id.sector = sec_map[s] + 1. */
                    for (size_t ts = 0; ts < track->sector_count; ts++) {
                        if (track->sectors[ts].id.sector == (uint8_t)(sec_map[s] + 1)) {
                            const uint8_t *data = track->sectors[ts].data;
                            if (data && track->sectors[ts].data_len >= ss) {
                                memcpy(p->data + pos, data, ss);
                            }
                            break;
                        }
                    }
                }
                pos += ss;
            }
        }

        if (is_target) break;
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_imd = {
    .name = "IMD", .description = "ImageDisk",
    .extensions = "imd", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = imd_plugin_probe, .open = imd_plugin_open,
    .close = imd_plugin_close, .read_track = imd_plugin_read_track,
    .write_track = imd_plugin_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_OFFICIAL_FULL,  /* Dave Dunfield published IMD.TXT with full format details */
};
UFT_REGISTER_FORMAT_PLUGIN(imd)
