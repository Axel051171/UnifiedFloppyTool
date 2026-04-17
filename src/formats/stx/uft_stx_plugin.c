/**
 * @file uft_stx_plugin.c
 * @brief STX (Pasti) Plugin-B — Atari ST protected format
 *
 * STX preserves sector timing and fuzzy bit masks for copy protection.
 * Magic: "RSY\0" (4 bytes). 16-byte file header + track descriptors.
 *
 * Reference: Pasti specification (Jean Louis-Guerin)
 */
#include "uft/uft_format_common.h"

#define STX_MAGIC_0 'R'
#define STX_MAGIC_1 'S'
#define STX_MAGIC_2 'Y'
#define STX_MAGIC_3 '\0'
#define STX_HEADER_SIZE 16
#define STX_MAX_TRACKS 200

typedef struct {
    uint8_t  *file_data;
    size_t    file_size;
    uint16_t  version;
    uint16_t  track_count;
    uint32_t  track_offsets[STX_MAX_TRACKS];
} stx_pd_t;

static bool stx_plugin_probe(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    (void)file_size;
    if (size < STX_HEADER_SIZE) return false;
    if (data[0] == STX_MAGIC_0 && data[1] == STX_MAGIC_1 &&
        data[2] == STX_MAGIC_2 && data[3] == STX_MAGIC_3) {
        *confidence = 97;
        return true;
    }
    return false;
}

static uft_error_t stx_open(uft_disk_t *disk, const char *path, bool ro) {
    (void)ro;
    uft_error_t err = UFT_ERROR_NO_MEMORY;
    size_t raw_size = 0;
    uint8_t *raw = uft_read_file(path, &raw_size);
    if (!raw) return UFT_ERROR_FILE_OPEN;
    if (raw_size < STX_HEADER_SIZE) { err = UFT_ERROR_FORMAT_INVALID; goto fail; }
    if (raw[0] != STX_MAGIC_0 || raw[1] != STX_MAGIC_1) { err = UFT_ERROR_FORMAT_INVALID; goto fail; }

    stx_pd_t *p = calloc(1, sizeof(stx_pd_t));
    if (!p) goto fail;

    p->file_data = raw;
    p->file_size = raw_size;
    p->version = uft_read_le16(raw + 4);
    p->track_count = uft_read_le16(raw + 10);
    if (p->track_count > STX_MAX_TRACKS) p->track_count = STX_MAX_TRACKS;

    /* Parse track offset table starting after header */
    size_t pos = STX_HEADER_SIZE;
    for (int t = 0; t < p->track_count && pos + 16 <= raw_size; t++) {
        p->track_offsets[t] = (uint32_t)pos;
        uint32_t trk_size = uft_read_le32(raw + pos);
        if (trk_size < 16 || pos + trk_size > raw_size) break;
        pos += trk_size;
    }

    disk->plugin_data = p;
    disk->geometry.cylinders = (p->track_count + 1) / 2;
    disk->geometry.heads = 2;
    disk->geometry.sectors = 9;
    disk->geometry.sector_size = 512;
    disk->geometry.total_sectors = (uint32_t)disk->geometry.cylinders * 2 * 9;
    return UFT_OK;

fail:
    free(raw);
    return err;
}

static void stx_close(uft_disk_t *disk) {
    stx_pd_t *p = disk->plugin_data;
    if (p) { free(p->file_data); free(p); disk->plugin_data = NULL; }
}

static uft_error_t stx_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track) {
    stx_pd_t *p = disk->plugin_data;
    if (!p || !p->file_data) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);

    int trk_idx = cyl * 2 + head;
    if (trk_idx >= p->track_count) return UFT_OK;

    uint32_t trk_off = p->track_offsets[trk_idx];
    if (trk_off + 16 > p->file_size) return UFT_OK;

    /* Track descriptor: size(4) + fuzzy_count(4) + sector_count(2) + flags(2) + ... */
    uint16_t sec_count = uft_read_le16(p->file_data + trk_off + 8);
    uint16_t trk_flags = uft_read_le16(p->file_data + trk_off + 10);
    (void)trk_flags;

    /* Sector descriptors start at offset 16 within track, 16 bytes each */
    size_t sec_desc_off = trk_off + 16;
    /* Sector data follows after all descriptors */
    size_t data_off = sec_desc_off + (size_t)sec_count * 16;

    for (int s = 0; s < sec_count && s < 26; s++) {
        size_t desc = sec_desc_off + (size_t)s * 16;
        if (desc + 16 > p->file_size) break;

        uint32_t data_offset = uft_read_le32(p->file_data + desc);
        uint16_t bit_pos = uft_read_le16(p->file_data + desc + 4);
        uint16_t read_time = uft_read_le16(p->file_data + desc + 6);
        uint8_t  sec_id = p->file_data[desc + 8];
        uint8_t  sec_n = p->file_data[desc + 11];
        uint8_t  fdcr = p->file_data[desc + 12];
        (void)bit_pos; (void)read_time;

        uint16_t sec_size = (sec_n < 4) ? (128 << sec_n) : 512;
        size_t abs_data = data_off + data_offset;
        if (abs_data + sec_size > p->file_size) continue;

        uft_format_add_sector(track, sec_id > 0 ? sec_id - 1 : 0,
                              p->file_data + abs_data, sec_size,
                              (uint8_t)cyl, (uint8_t)head);

        /* Mark CRC errors from FDC status */
        if ((fdcr & 0x08) && track->sector_count > 0)
            track->sectors[track->sector_count - 1].crc_ok = false;
    }
    return UFT_OK;
}

/* NOTE: write_track omitted by design — STX stores fuzzy-bit streams and
 * per-sector timing that cannot be regenerated from sector data alone.
 * The Pasti format is purpose-built to preserve ST copy-protection, so
 * round-tripping through a sector-level write would destroy protection. */
const uft_format_plugin_t uft_format_plugin_stx = {
    .name = "STX", .description = "Atari ST Pasti (Protected)",
    .extensions = "stx", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_TIMING,
    .probe = stx_plugin_probe, .open = stx_open,
    .close = stx_close, .read_track = stx_read_track,
};
UFT_REGISTER_FORMAT_PLUGIN(stx)
