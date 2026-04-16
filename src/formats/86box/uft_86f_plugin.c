/**
 * @file uft_86f_plugin.c
 * @brief 86F (86Box/PCem) Plugin-B wrapper
 *
 * 86F is a track-level format used by 86Box and PCem emulators.
 * Magic: "86BX" at offset 0, followed by geometry and track data.
 *
 * Wraps existing uft_86f_probe() / uft_86f_read() API.
 */
#include "uft/uft_format_common.h"

#define F86_MAGIC       "86BX"
#define F86_MAGIC_LEN   4
#define F86_HDR_SIZE    32

typedef struct {
    uint8_t *data;
    size_t   size;
    uint8_t  tracks;
    uint8_t  sides;
    uint8_t  disk_type;
} f86_pd_t;

/* Geometry from disk type byte */
static void f86_geometry(uint8_t dtype, uint8_t *cyl, uint8_t *heads,
                          uint8_t *spt, uint16_t *ss) {
    switch (dtype) {
        case 0x00: *cyl = 40; *heads = 2; *spt = 9;  *ss = 512; break; /* 360K */
        case 0x01: *cyl = 80; *heads = 2; *spt = 9;  *ss = 512; break; /* 720K */
        case 0x02: *cyl = 80; *heads = 2; *spt = 15; *ss = 512; break; /* 1.2M */
        case 0x03: *cyl = 80; *heads = 2; *spt = 18; *ss = 512; break; /* 1.44M */
        case 0x04: *cyl = 80; *heads = 2; *spt = 36; *ss = 512; break; /* 2.88M */
        default:   *cyl = 80; *heads = 2; *spt = 18; *ss = 512; break;
    }
}

static bool f86_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)fs;
    if (s < F86_HDR_SIZE) return false;
    if (memcmp(d, F86_MAGIC, F86_MAGIC_LEN) == 0) {
        *c = 98;
        return true;
    }
    return false;
}

static uft_error_t f86_open(uft_disk_t *disk, const char *path, bool ro) {
    (void)ro;
    size_t file_size = 0;
    uint8_t *data = uft_read_file(path, &file_size);
    if (!data || file_size < F86_HDR_SIZE) { free(data); return UFT_ERROR_FILE_OPEN; }
    if (memcmp(data, F86_MAGIC, F86_MAGIC_LEN) != 0) {
        free(data); return UFT_ERROR_FORMAT_INVALID;
    }

    f86_pd_t *p = calloc(1, sizeof(f86_pd_t));
    if (!p) { free(data); return UFT_ERROR_NO_MEMORY; }
    p->data = data;
    p->size = file_size;
    p->disk_type = data[6];
    p->tracks = data[8];
    p->sides = data[7];

    uint8_t cyl, heads, spt;
    uint16_t ss;
    f86_geometry(p->disk_type, &cyl, &heads, &spt, &ss);
    if (p->tracks > 0) cyl = p->tracks;
    if (p->sides > 0) heads = p->sides;

    disk->plugin_data = p;
    disk->geometry.cylinders = cyl;
    disk->geometry.heads = heads;
    disk->geometry.sectors = spt;
    disk->geometry.sector_size = ss;
    disk->geometry.total_sectors = (uint32_t)cyl * heads * spt;
    return UFT_OK;
}

static void f86_close(uft_disk_t *disk) {
    f86_pd_t *p = disk->plugin_data;
    if (p) { free(p->data); free(p); disk->plugin_data = NULL; }
}

static uft_error_t f86_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track) {
    f86_pd_t *p = disk->plugin_data;
    if (!p || !p->data) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    /* 86F track table starts at offset 32, each entry is 12 bytes:
     * offset(4LE) + length(4LE) + flags(1) + sectors(1) + rpm(2LE) */
    int idx = cyl * (p->sides > 0 ? p->sides : 2) + head;
    size_t entry_off = F86_HDR_SIZE + (size_t)idx * 12;
    if (entry_off + 12 > p->size) return UFT_OK;

    uint32_t trk_off = uft_read_le32(p->data + entry_off);
    uint32_t trk_len = uft_read_le32(p->data + entry_off + 4);
    uint8_t flags = p->data[entry_off + 8];
    uint8_t nsec = p->data[entry_off + 9];

    if (trk_off == 0 || trk_off + trk_len > p->size) return UFT_OK;
    if (!(flags & 0x01)) return UFT_OK; /* Track not valid */

    /* 86F sector data: parse MFM-encoded track or raw sectors
     * For simplicity, treat as raw sector data if nsec > 0 */
    if (nsec > 0 && trk_len >= (uint32_t)nsec * disk->geometry.sector_size) {
        size_t pos = trk_off;
        for (int s = 0; s < nsec && pos + disk->geometry.sector_size <= p->size; s++) {
            uft_format_add_sector(track, (uint8_t)s,
                                  p->data + pos, disk->geometry.sector_size,
                                  (uint8_t)cyl, (uint8_t)head);
            pos += disk->geometry.sector_size;
        }
    } else {
        /* Store raw track data as sector 0 */
        uint16_t chunk = (trk_len > 65535) ? 65535 : (uint16_t)trk_len;
        uft_format_add_sector(track, 0, p->data + trk_off, chunk,
                              (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

static uft_error_t f86_write_track(uft_disk_t *disk, int cyl, int head,
                                    const uft_track_t *track) {
    f86_pd_t *p = disk->plugin_data;
    if (!p || !p->data) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    int idx = cyl * (p->sides > 0 ? p->sides : 2) + head;
    size_t entry_off = F86_HDR_SIZE + (size_t)idx * 12;
    if (entry_off + 12 > p->size) return UFT_ERROR_INVALID_ARG;

    uint32_t trk_off = uft_read_le32(p->data + entry_off);
    uint32_t trk_len = uft_read_le32(p->data + entry_off + 4);
    uint8_t flags = p->data[entry_off + 8];
    uint8_t nsec = p->data[entry_off + 9];

    if (trk_off == 0 || trk_off + trk_len > p->size) return UFT_ERROR_INVALID_ARG;
    if (!(flags & 0x01)) return UFT_ERROR_INVALID_ARG;

    if (nsec > 0 && trk_len >= (uint32_t)nsec * disk->geometry.sector_size) {
        size_t pos = trk_off;
        for (int s = 0; s < nsec && pos + disk->geometry.sector_size <= p->size; s++) {
            /* Find matching sector in input track */
            for (size_t ts = 0; ts < track->sector_count; ts++) {
                if (track->sectors[ts].id.sector == (uint8_t)s) {
                    const uint8_t *src = track->sectors[ts].data;
                    if (src && track->sectors[ts].data_len >= disk->geometry.sector_size)
                        memcpy(p->data + pos, src, disk->geometry.sector_size);
                    break;
                }
            }
            pos += disk->geometry.sector_size;
        }
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_86f = {
    .name = "86F", .description = "86Box/PCem Floppy Image",
    .extensions = "86f", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_FLUX,
    .probe = f86_probe, .open = f86_open,
    .close = f86_close, .read_track = f86_read_track,
    .write_track = f86_write_track,
};
UFT_REGISTER_FORMAT_PLUGIN(86f)
