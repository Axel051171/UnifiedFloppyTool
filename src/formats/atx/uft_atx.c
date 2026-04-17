/**
 * @file uft_atx.c
 * @brief ATX (Atari 8-bit Protected) Plugin-B
 *
 * ATX stores Atari 8-bit disk data with FDC status, timing, and weak
 * bit information — critical for copy protection preservation.
 *
 * Header: "AT8X" (0x41543858 LE), version, track count, chunk table.
 * Each track: sector list chunk + data chunk + optional weak bits.
 *
 * Reference: VAPI (Vinculatum Atari Preservation Initiative) specification
 */
#include "uft/uft_format_common.h"

#define ATX_SIGNATURE   0x41543858u  /* "AT8X" LE */
#define ATX_HEADER_SIZE 48
#define ATX_MAX_TRACKS  42
#define ATX_MAX_SECTORS 26
#define ATX_SECTOR_SIZE 128  /* SD default, DD uses 256 */

typedef struct {
    uint8_t  *file_data;
    size_t    file_size;
    uint16_t  version;
    uint8_t   density;     /* 0=SD(128), 1=ED(128), 2=DD(256) */
    uint16_t  sector_size;
    uint8_t   track_count;
    uint32_t  track_offsets[ATX_MAX_TRACKS];
} atx_data_t;

static bool atx_plugin_probe(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    (void)file_size;
    if (size < ATX_HEADER_SIZE) return false;
    uint32_t sig = uft_read_le32(data);
    if (sig == ATX_SIGNATURE) {
        *confidence = 98;
        return true;
    }
    return false;
}

static uft_error_t atx_open(uft_disk_t *disk, const char *path, bool ro) {
    (void)ro;
    uft_error_t err = UFT_ERROR_NO_MEMORY;
    uint8_t *raw = NULL;
    atx_data_t *p = NULL;

    size_t raw_size = 0;
    raw = uft_read_file(path, &raw_size);
    if (!raw) return UFT_ERROR_FILE_OPEN;
    if (raw_size < ATX_HEADER_SIZE) { err = UFT_ERROR_FORMAT_INVALID; goto fail; }

    uint32_t sig = uft_read_le32(raw);
    if (sig != ATX_SIGNATURE) { err = UFT_ERROR_FORMAT_INVALID; goto fail; }

    p = calloc(1, sizeof(atx_data_t));
    if (!p) goto fail;

    p->file_data = raw;
    p->file_size = raw_size;
    p->version = uft_read_le16(raw + 4);
    p->density = raw[19];
    p->sector_size = (p->density >= 2) ? 256 : 128;
    p->track_count = raw[20];
    if (p->track_count > ATX_MAX_TRACKS) p->track_count = ATX_MAX_TRACKS;

    /* Parse track offset table (starts at offset 48) */
    for (int t = 0; t < p->track_count && 48 + t * 4 + 4 <= raw_size; t++) {
        p->track_offsets[t] = uft_read_le32(raw + 48 + t * 4);
    }

    disk->plugin_data = p;
    disk->geometry.cylinders = p->track_count;
    disk->geometry.heads = 1;
    disk->geometry.sectors = 18; /* typical Atari */
    disk->geometry.sector_size = p->sector_size;
    disk->geometry.total_sectors = (uint32_t)p->track_count * 18;
    return UFT_OK;

fail:
    free(p);
    free(raw);
    return err;
}

static void atx_close(uft_disk_t *disk) {
    atx_data_t *p = disk->plugin_data;
    if (p) { free(p->file_data); free(p); disk->plugin_data = NULL; }
}

static uft_error_t atx_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track) {
    atx_data_t *p = disk->plugin_data;
    if (!p || !p->file_data) return UFT_ERROR_INVALID_STATE;
    if (head != 0 || cyl >= p->track_count) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    uint32_t trk_off = p->track_offsets[cyl];
    if (trk_off == 0 || trk_off + 8 > p->file_size) return UFT_OK;

    /* Track record: size(4) + type(2) + reserved(2) + sector_count(2) + ... */
    uint32_t trk_size = uft_read_le32(p->file_data + trk_off);
    if (trk_off + trk_size > p->file_size) return UFT_OK;

    /* Scan chunks within track */
    size_t pos = trk_off + 8; /* skip track header */
    uint8_t sec_count = 0;
    uint32_t sec_list_off = 0, sec_data_off = 0;

    /* Find sector list and data chunks */
    while (pos + 8 <= trk_off + trk_size) {
        uint32_t chunk_size = uft_read_le32(p->file_data + pos);
        uint16_t chunk_type = uft_read_le16(p->file_data + pos + 4);

        if (chunk_type == 0x0001) { /* Sector List */
            sec_count = p->file_data[pos + 8];
            sec_list_off = (uint32_t)pos + 12;
        } else if (chunk_type == 0x0002) { /* Sector Data */
            sec_data_off = (uint32_t)pos + 8;
        }

        if (chunk_size < 8) break;
        pos += chunk_size;
    }

    if (sec_count == 0 || sec_list_off == 0 || sec_data_off == 0)
        return UFT_OK;

    /* Read sectors from list + data */
    for (int s = 0; s < sec_count && s < ATX_MAX_SECTORS; s++) {
        size_t entry_off = sec_list_off + (size_t)s * 8;
        if (entry_off + 8 > p->file_size) break;

        uint8_t sec_num = p->file_data[entry_off];
        uint8_t fdc_status = p->file_data[entry_off + 1];
        uint32_t data_off = uft_read_le32(p->file_data + entry_off + 4);

        size_t abs_data = sec_data_off + data_off;
        if (abs_data + p->sector_size > p->file_size) continue;

        uft_format_add_sector(track,
            sec_num > 0 ? sec_num - 1 : 0,
            p->file_data + abs_data, p->sector_size,
            (uint8_t)cyl, 0);

        /* Mark sectors with FDC errors */
        if (fdc_status & 0x08) { /* CRC error */
            if (track->sector_count > 0)
                track->sectors[track->sector_count - 1].crc_ok = false;
        }
    }
    return UFT_OK;
}

/* NOTE: write_track omitted by design — ATX encodes per-sector timing
 * anomalies (weak bits, duplicate sector IDs, fuzzy bits, copy-protection
 * signatures) that cannot be synthesized from sector payload alone. */
const uft_format_plugin_t uft_format_plugin_atx = {
    .name = "ATX", .description = "Atari 8-bit Protected (VAPI)",
    .extensions = "atx", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WEAK_BITS |
                    UFT_FORMAT_CAP_VERIFY,
    .probe = atx_plugin_probe, .open = atx_open,
    .close = atx_close, .read_track = atx_read_track,
    .verify_track = uft_weak_bit_verify_track,
};
UFT_REGISTER_FORMAT_PLUGIN(atx)
