/**
 * @file uft_d64_plugin.c
 * @brief D64 (Commodore 1541) Plugin-B — self-contained
 *
 * D64: headerless raw sector dump, 256 bytes/sector.
 * 35 tracks (174848 bytes) or 40 tracks (196608 bytes).
 * Variable sectors per track: 21/19/18/17 for density zones.
 * Optional 683-byte error info block at end.
 */
#include "uft/uft_format_common.h"

static const uint8_t d64_spt[] = {
    0,
    21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21, /* 1-17 */
    19,19,19,19,19,19,19,                                 /* 18-24 */
    18,18,18,18,18,18,                                     /* 25-30 */
    17,17,17,17,17,17,17,17,17,17,                        /* 31-40 */
    17,17                                                 /* 41-42 (non-standard) */
};

#define D64_MAX_TRACK 42

/* Byte offset for track T (1-based), sector S (0-based) */
static long d64_offset(int track, int sector) {
    long off = 0;
    for (int t = 1; t < track && t <= D64_MAX_TRACK; t++)
        off += d64_spt[t] * 256;
    return off + sector * 256;
}

typedef struct {
    FILE *file;
    int  max_track;
    bool has_errors;   /* trailing 1541 error-info block present (.d64 +errors) */
    long err_offset;   /* byte offset of the error block = total sector bytes */
} d64_pd_t;

static bool d64_plugin_probe(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    /* D64 sizes: 35trk 174848/175531(+err), 40trk 196608/197376(+err),
     * plus the non-standard extended variants (VICE/Schepers):
     * 41trk 200960/201745(+err), 42trk 205312/206114(+err). */
    if (file_size != 174848 && file_size != 175531 &&
        file_size != 196608 && file_size != 197376 &&
        file_size != 200960 && file_size != 201745 &&
        file_size != 205312 && file_size != 206114)
        return false;
    *confidence = 75;

    /* BAM at track 18 sector 0 (offset 0x16500 for 35-track) */
    if (size >= 0x16600) {
        /* BAM starts with track/sector link to directory (usually 18/1) */
        if (data[0x16500] == 18 && data[0x16501] == 1) *confidence = 92;
        /* DOS version byte at 0x16502: 'A' = CBM DOS 2.6 */
        else if (data[0x16502] == 0x41) *confidence = 88;
    }
    return true;
}

static uft_error_t d64_plugin_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    d64_pd_t *p = calloc(1, sizeof(d64_pd_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    /* Track count from size: extended 41/42-track variants supported additively.
     * The +error sizes stay below the next track threshold, so >= works. */
    if      (sz >= 205312) p->max_track = 42;
    else if (sz >= 200960) p->max_track = 41;
    else if (sz >= 196608) p->max_track = 40;
    else                   p->max_track = 35;

    disk->plugin_data = p;
    disk->geometry.cylinders = p->max_track;
    disk->geometry.heads = 1;
    disk->geometry.sectors = 21;
    disk->geometry.sector_size = 256;

    /* Count total sectors */
    uint32_t total = 0;
    for (int t = 1; t <= p->max_track; t++) total += d64_spt[t];
    disk->geometry.total_sectors = total;

    /* A .d64 with a trailing error-info block has one error byte per sector
     * appended after the sector data (175531 = 35trk+683, 197376 = 40trk+768).
     * Detect it so read_track can surface the 1541 controller error codes on
     * the sectors instead of silently dropping them. */
    p->err_offset = (long)total * 256;
    p->has_errors = (sz >= p->err_offset + (long)total);
    return UFT_OK;
}

static void d64_plugin_close(uft_disk_t *disk) {
    d64_pd_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t d64_plugin_read_track(uft_disk_t *disk, int cyl, int head,
                                          uft_track_t *track) {
    d64_pd_t *p = disk->plugin_data;
    if (!p || !p->file || head != 0) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    int d64_track = cyl + 1;
    if (d64_track < 1 || d64_track > p->max_track) return UFT_OK;
    int nsectors = d64_spt[d64_track];

    uint8_t buf[256];
    for (int s = 0; s < nsectors; s++) {
        long off = d64_offset(d64_track, s);
        if (fseek(p->file, off, SEEK_SET) != 0) continue;
        if (fread(buf, 1, 256, p->file) != 256) continue;
        uft_format_add_sector(track, (uint8_t)s, buf, 256,
                              (uint8_t)cyl, (uint8_t)head);

        /* Surface the 1541 error code for this sector (represent, don't drop).
         * Code 0x01 = "00, OK"; anything else (02=header not found, 04=data
         * not found, 05=data checksum error, ...) is a read error → mark the
         * sector CRC-bad so consumers see the original media defect. */
        if (p->has_errors && track->sector_count > 0) {
            long ei = p->err_offset + (off / 256);
            uint8_t code = 0x01;
            if (fseek(p->file, ei, SEEK_SET) == 0 &&
                fread(&code, 1, 1, p->file) == 1 &&
                code != 0x00 && code != 0x01) {
                uft_sector_set_crc(&track->sectors[track->sector_count - 1],
                                   false);
            }
        }
    }
    return UFT_OK;
}

static uft_error_t d64_plugin_write_track(uft_disk_t *disk, int cyl, int head,
                                           const uft_track_t *track) {
    d64_pd_t *p = disk->plugin_data;
    if (!p || !p->file || head != 0) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    int d64_track = cyl + 1;
    if (d64_track < 1 || d64_track > p->max_track) return UFT_OK;
    int nsectors = d64_spt[d64_track];

    for (size_t s = 0; s < track->sector_count && (int)s < nsectors; s++) {
        long off = d64_offset(d64_track, (int)s);
        if (fseek(p->file, off, SEEK_SET) != 0) return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        size_t len = track->sectors[s].data_len;
        uint8_t pad[256];
        if (!data || len == 0) {
            memset(pad, 0xE5, 256); data = pad; len = 256;
        } else if (len < 256) {
            memset(pad, 0xE5, 256); memcpy(pad, data, len); data = pad; len = 256;
        }
        if (fwrite(data, 1, 256, p->file) != 256) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_d64_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_d64 = {
    .name = "D64", .description = "Commodore 1541 D64",
    .extensions = "d64", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = d64_plugin_probe, .open = d64_plugin_open,
    .close = d64_plugin_close, .read_track = d64_plugin_read_track,
    .write_track = d64_plugin_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_DERIVED,  /* 1541 DOS well-known but never formally specced; de-facto via VICE */
    .features = uft_format_plugin_d64_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_d64_features) / sizeof(uft_format_plugin_d64_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(d64)
