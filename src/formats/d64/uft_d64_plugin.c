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
    17,17,17,17,17,17,17,17,17,17                          /* 31-40 */
};

/* Byte offset for track T (1-based), sector S (0-based) */
static long d64_offset(int track, int sector) {
    long off = 0;
    for (int t = 1; t < track && t <= 40; t++)
        off += d64_spt[t] * 256;
    return off + sector * 256;
}

typedef struct { FILE *file; int max_track; } d64_pd_t;

static bool d64_plugin_probe(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    /* D64 sizes: 174848 (35trk), 175531 (35+err), 196608 (40trk), 197376 (40+err) */
    if (file_size != 174848 && file_size != 175531 &&
        file_size != 196608 && file_size != 197376)
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
    p->max_track = (sz >= 196608) ? 40 : 35;

    disk->plugin_data = p;
    disk->geometry.cylinders = p->max_track;
    disk->geometry.heads = 1;
    disk->geometry.sectors = 21;
    disk->geometry.sector_size = 256;

    /* Count total sectors */
    uint32_t total = 0;
    for (int t = 1; t <= p->max_track; t++) total += d64_spt[t];
    disk->geometry.total_sectors = total;
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
            memset(pad, 0, 256); data = pad; len = 256;
        } else if (len < 256) {
            memset(pad, 0, 256); memcpy(pad, data, len); data = pad; len = 256;
        }
        if (fwrite(data, 1, 256, p->file) != 256) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_d64 = {
    .name = "D64", .description = "Commodore 1541 D64",
    .extensions = "d64", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = d64_plugin_probe, .open = d64_plugin_open,
    .close = d64_plugin_close, .read_track = d64_plugin_read_track,
    .write_track = d64_plugin_write_track,
};
UFT_REGISTER_FORMAT_PLUGIN(d64)
