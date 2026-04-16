/**
 * @file uft_msa_plugin.c
 * @brief MSA (Atari ST compressed) Plugin-B wrapper
 *
 * Decompresses MSA to raw ST image in memory, then serves sectors.
 */
#include "uft/uft_format_common.h"

/* From uft_msa.c */
extern int uft_msa_probe(const uint8_t *data, size_t len);

/* MSA header: bytes 0-1 = 0x0E0F magic, 2-3 = spt, 4-5 = sides,
 * 6-7 = start_track, 8-9 = end_track */
#define MSA_MAGIC 0x0E0F

typedef struct {
    uint8_t *st_data;   /* Decompressed raw ST image */
    size_t   st_size;
    uint8_t  cyl;
    uint8_t  heads;
    uint8_t  spt;
} msa_plugin_data_t;

/* Simple MSA RLE decompressor (marker 0xE5) */
static size_t msa_decompress_track(const uint8_t *src, size_t src_len,
                                    uint8_t *dst, size_t track_size)
{
    size_t sp = 0, dp = 0;
    while (sp < src_len && dp < track_size) {
        if (src[sp] == 0xE5 && sp + 3 < src_len) {
            uint8_t val = src[sp + 1];
            uint16_t count = ((uint16_t)src[sp + 2] << 8) | src[sp + 3];
            sp += 4;
            for (uint16_t i = 0; i < count && dp < track_size; i++)
                dst[dp++] = val;
        } else {
            dst[dp++] = src[sp++];
        }
    }
    return dp;
}

static bool msa_plugin_probe(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    (void)file_size;
    if (size < 10) return false;
    uint16_t magic = ((uint16_t)data[0] << 8) | data[1];
    if (magic == MSA_MAGIC) { *confidence = 95; return true; }
    return false;
}

static uft_error_t msa_plugin_open(uft_disk_t *disk, const char *path, bool ro) {
    (void)ro;
    size_t file_size = 0;
    uint8_t *msa = uft_read_file(path, &file_size);
    if (!msa || file_size < 10) { free(msa); return UFT_ERROR_FORMAT_INVALID; }

    uint16_t magic = ((uint16_t)msa[0] << 8) | msa[1];
    if (magic != MSA_MAGIC) { free(msa); return UFT_ERROR_FORMAT_INVALID; }

    uint16_t spt = ((uint16_t)msa[2] << 8) | msa[3];
    uint16_t sides = (((uint16_t)msa[4] << 8) | msa[5]) + 1;
    uint16_t start_trk = ((uint16_t)msa[6] << 8) | msa[7];
    uint16_t end_trk = ((uint16_t)msa[8] << 8) | msa[9];

    if (spt == 0 || spt > 18 || sides == 0 || sides > 2 || end_trk < start_trk) {
        free(msa); return UFT_ERROR_FORMAT_INVALID;
    }

    uint8_t cyl = (uint8_t)(end_trk + 1);
    size_t track_size = (size_t)spt * 512;
    size_t st_size = (size_t)cyl * sides * track_size;
    uint8_t *st = calloc(1, st_size);
    if (!st) { free(msa); return UFT_ERROR_NO_MEMORY; }

    /* Decompress each track */
    size_t pos = 10;
    for (int t = start_trk; t <= end_trk; t++) {
        for (int s = 0; s < (int)sides; s++) {
            if (pos + 2 > file_size) break;
            uint16_t data_len = ((uint16_t)msa[pos] << 8) | msa[pos + 1];
            pos += 2;
            if (pos + data_len > file_size) break;

            size_t dst_off = ((size_t)t * sides + s) * track_size;
            if (data_len == track_size) {
                /* Uncompressed */
                memcpy(st + dst_off, msa + pos, track_size);
            } else {
                /* RLE compressed */
                msa_decompress_track(msa + pos, data_len, st + dst_off, track_size);
            }
            pos += data_len;
        }
    }
    free(msa);

    msa_plugin_data_t *p = calloc(1, sizeof(msa_plugin_data_t));
    if (!p) { free(st); return UFT_ERROR_NO_MEMORY; }
    p->st_data = st; p->st_size = st_size;
    p->cyl = cyl; p->heads = (uint8_t)sides; p->spt = (uint8_t)spt;

    disk->plugin_data = p;
    disk->geometry.cylinders = cyl;
    disk->geometry.heads = sides;
    disk->geometry.sectors = spt;
    disk->geometry.sector_size = 512;
    disk->geometry.total_sectors = (uint32_t)cyl * sides * spt;
    return UFT_OK;
}

static void msa_plugin_close(uft_disk_t *disk) {
    msa_plugin_data_t *p = disk->plugin_data;
    if (p) { free(p->st_data); free(p); disk->plugin_data = NULL; }
}

static uft_error_t msa_plugin_read_track(uft_disk_t *disk, int cyl, int head,
                                          uft_track_t *track) {
    msa_plugin_data_t *p = disk->plugin_data;
    if (!p || !p->st_data) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);

    size_t off = ((size_t)cyl * p->heads + head) * p->spt * 512;
    uint8_t buf[512];
    for (int s = 0; s < p->spt; s++) {
        size_t soff = off + (size_t)s * 512;
        if (soff + 512 > p->st_size) break;
        memcpy(buf, p->st_data + soff, 512);
        uft_format_add_sector(track, (uint8_t)s, buf, 512, (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_msa = {
    .name = "MSA", .description = "Atari ST Compressed (Magic Shadow)",
    .extensions = "msa", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ,
    .probe = msa_plugin_probe, .open = msa_plugin_open,
    .close = msa_plugin_close, .read_track = msa_plugin_read_track,
};
UFT_REGISTER_FORMAT_PLUGIN(msa)
