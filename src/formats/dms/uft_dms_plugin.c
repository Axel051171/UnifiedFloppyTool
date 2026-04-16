/**
 * @file uft_dms_plugin.c
 * @brief DMS (Disk Masher System) Plugin-B — Amiga compressed disk format
 *
 * DMS header: "DMS!" magic at offset 0, followed by 36-byte info header.
 * Track records follow, each with compressed or uncompressed data.
 *
 * Compression types supported:
 *   0 = Uncompressed (store)
 *   1 = Simple RLE
 *   2+ = Quick/Medium/Heavy/Deep (unsupported, filled with 0xE5)
 *
 * Amiga disk geometry: 80 cyl x 2 heads x 11 spt x 512 = 901120 bytes
 *
 * Reference: xDMS source, dms2adf, AROS source
 */
#include "uft/uft_format_common.h"

#define DMS_MAGIC       "DMS!"
#define DMS_HEADER_SIZE 56          /* 4 magic + 52 info header */
#define DMS_TRACK_HDR   20          /* track record header size */
#define AMIGA_TRACK_SIZE (11 * 512) /* 5632 bytes per track */
#define AMIGA_CYL       80
#define AMIGA_HEADS      2
#define AMIGA_SPT       11
#define AMIGA_SS        512

typedef struct {
    uint8_t *adf;       /* Decompressed raw ADF image */
    size_t   adf_size;
} dms_pd_t;

/* DMS RLE decompressor (type 1).
 * Simple: bytes are literal unless escape 0x90 followed by count+byte. */
static size_t dms_rle_decompress(const uint8_t *src, size_t src_len,
                                  uint8_t *dst, size_t dst_cap)
{
    size_t sp = 0, dp = 0;
    while (sp < src_len && dp < dst_cap) {
        uint8_t b = src[sp++];
        if (b == 0x90) {
            if (sp >= src_len) break;
            uint8_t count = src[sp++];
            if (count == 0) {
                /* Literal 0x90 */
                dst[dp++] = 0x90;
            } else {
                if (sp >= src_len) break;
                uint8_t val = src[sp++];
                for (int i = 0; i < count && dp < dst_cap; i++)
                    dst[dp++] = val;
            }
        } else {
            dst[dp++] = b;
        }
    }
    return dp;
}

static bool dms_probe(const uint8_t *data, size_t size, size_t file_size,
                       int *confidence)
{
    (void)file_size;
    if (size < 4) return false;
    if (memcmp(data, DMS_MAGIC, 4) == 0) {
        *confidence = 98;
        return true;
    }
    return false;
}

static uft_error_t dms_open(uft_disk_t *disk, const char *path, bool ro)
{
    (void)ro;
    size_t file_size = 0;
    uint8_t *raw = uft_read_file(path, &file_size);
    if (!raw || file_size < DMS_HEADER_SIZE) {
        free(raw);
        return UFT_ERROR_FILE_OPEN;
    }

    if (memcmp(raw, DMS_MAGIC, 4) != 0) {
        free(raw);
        return UFT_ERROR_FORMAT_INVALID;
    }

    /* Allocate ADF buffer: 80 cyl x 2 heads x 11 spt x 512 */
    size_t adf_size = (size_t)AMIGA_CYL * AMIGA_HEADS * AMIGA_TRACK_SIZE;
    uint8_t *adf = malloc(adf_size);
    if (!adf) { free(raw); return UFT_ERROR_NO_MEMORY; }
    memset(adf, 0xE5, adf_size);  /* Forensic fill */

    /* Parse track records starting after DMS header.
     * DMS info header is at offset 4, length 52 bytes.
     * Track data starts at offset 56. */
    size_t pos = DMS_HEADER_SIZE;

    while (pos + DMS_TRACK_HDR <= file_size) {
        /* Track header:
         *  0-1: "TR" marker (big-endian)
         *  2-3: track number (big-endian)
         *  4-5: unknown/reserved
         *  6-7: packed CRC (big-endian)
         *  8-9: unpacked CRC (big-endian)
         * 10-11: compression mode (big-endian)
         * 12-15: unpacked size (big-endian 32)
         * 16-19: packed size (big-endian 32)
         */
        uint8_t *th = raw + pos;

        /* Check for TR marker */
        if (th[0] != 'T' || th[1] != 'R') {
            /* Try scanning forward for next TR marker */
            pos++;
            continue;
        }

        uint16_t trk_num = ((uint16_t)th[2] << 8) | th[3];
        uint16_t comp_mode = ((uint16_t)th[10] << 8) | th[11];
        uint32_t unpacked_size = ((uint32_t)th[12] << 24) |
                                 ((uint32_t)th[13] << 16) |
                                 ((uint32_t)th[14] << 8) | th[15];
        uint32_t packed_size = ((uint32_t)th[16] << 24) |
                               ((uint32_t)th[17] << 16) |
                               ((uint32_t)th[18] << 8) | th[19];

        pos += DMS_TRACK_HDR;

        /* Bounds check */
        if (packed_size > file_size - pos) break;
        if (unpacked_size > AMIGA_TRACK_SIZE) unpacked_size = AMIGA_TRACK_SIZE;

        /* Destination offset in ADF */
        size_t dst_off = (size_t)trk_num * AMIGA_TRACK_SIZE;
        if (dst_off + unpacked_size > adf_size) {
            pos += packed_size;
            continue;
        }

        switch (comp_mode) {
            case 0:
                /* Uncompressed (store) */
                if (packed_size <= unpacked_size) {
                    memcpy(adf + dst_off, raw + pos,
                           packed_size < unpacked_size ? packed_size : unpacked_size);
                }
                break;
            case 1:
                /* Simple RLE */
                dms_rle_decompress(raw + pos, packed_size,
                                   adf + dst_off, unpacked_size);
                break;
            default:
                /* Quick/Medium/Heavy/Deep: not yet implemented.
                 * Track remains filled with 0xE5 forensic pattern. */
                break;
        }
        pos += packed_size;
    }
    free(raw);

    dms_pd_t *p = calloc(1, sizeof(dms_pd_t));
    if (!p) { free(adf); return UFT_ERROR_NO_MEMORY; }
    p->adf = adf;
    p->adf_size = adf_size;

    disk->plugin_data = p;
    disk->geometry.cylinders = AMIGA_CYL;
    disk->geometry.heads = AMIGA_HEADS;
    disk->geometry.sectors = AMIGA_SPT;
    disk->geometry.sector_size = AMIGA_SS;
    disk->geometry.total_sectors = (uint32_t)AMIGA_CYL * AMIGA_HEADS * AMIGA_SPT;
    return UFT_OK;
}

static void dms_close(uft_disk_t *disk)
{
    dms_pd_t *p = disk->plugin_data;
    if (p) {
        free(p->adf);
        free(p);
        disk->plugin_data = NULL;
    }
}

static uft_error_t dms_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track)
{
    dms_pd_t *p = disk->plugin_data;
    if (!p || !p->adf) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    size_t trk_off = ((size_t)cyl * AMIGA_HEADS + head) * AMIGA_TRACK_SIZE;
    for (int s = 0; s < AMIGA_SPT; s++) {
        size_t soff = trk_off + (size_t)s * AMIGA_SS;
        if (soff + AMIGA_SS > p->adf_size) break;
        uft_format_add_sector(track, (uint8_t)s, p->adf + soff,
                              AMIGA_SS, (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_dms = {
    .name = "DMS", .description = "Amiga DMS (Disk Masher System)",
    .extensions = "dms", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ,
    .probe = dms_probe, .open = dms_open, .close = dms_close,
    .read_track = dms_read_track,
};
UFT_REGISTER_FORMAT_PLUGIN(dms)
