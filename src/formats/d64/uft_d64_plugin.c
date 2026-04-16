/**
 * @file uft_d64_plugin.c
 * @brief D64 (Commodore 1541) Plugin-B wrapper
 *
 * Wraps the existing d64_open/close/read_sector API (4172 LOC in uft_d64.c)
 * into the format plugin interface for the central registry.
 */
#include "uft/uft_format_common.h"

/* External API from uft_d64.c / uft_d64_parser_v3.c */
typedef struct d64_image_s d64_image_t;
extern d64_image_t* d64_open(const char *path);
extern void d64_close(d64_image_t *img);
extern int d64_read_sector(const d64_image_t *img, int track, int sector,
                            uint8_t *buf);

/* D64 sectors per track (1-based index) */
static const uint8_t d64_spt[] = {
    0,
    21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21, /* 1-17 */
    19,19,19,19,19,19,19,                                 /* 18-24 */
    18,18,18,18,18,18,                                     /* 25-30 */
    17,17,17,17,17,17,17,17,17,17                          /* 31-40 */
};

typedef struct { d64_image_t *img; int max_track; } d64_pd_t;

static bool d64_plugin_probe(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    (void)data; (void)size;
    if (file_size == 174848 || file_size == 175531) { *confidence = 80; return true; }
    if (file_size == 196608 || file_size == 197376) { *confidence = 80; return true; }
    return false;
}

static uft_error_t d64_plugin_open(uft_disk_t *disk, const char *path, bool ro) {
    (void)ro;
    d64_image_t *img = d64_open(path);
    if (!img) return UFT_ERROR_FORMAT_INVALID;

    d64_pd_t *p = calloc(1, sizeof(d64_pd_t));
    if (!p) { d64_close(img); return UFT_ERROR_NO_MEMORY; }
    p->img = img;

    /* Detect 35 vs 40 tracks from file size */
    FILE *f = fopen(path, "rb");
    if (f) {
        if (fseek(f, 0, SEEK_END) == 0) {
            long sz = ftell(f);
            p->max_track = (sz >= 196608) ? 40 : 35;
        }
        fclose(f);
    }
    if (p->max_track == 0) p->max_track = 35;

    disk->plugin_data = p;
    disk->geometry.cylinders = p->max_track;
    disk->geometry.heads = 1;
    disk->geometry.sectors = 21;
    disk->geometry.sector_size = 256;
    disk->geometry.total_sectors = 683; /* 35-track standard */
    return UFT_OK;
}

static void d64_plugin_close(uft_disk_t *disk) {
    d64_pd_t *p = disk->plugin_data;
    if (p) { if (p->img) d64_close(p->img); free(p); disk->plugin_data = NULL; }
}

static uft_error_t d64_plugin_read_track(uft_disk_t *disk, int cyl, int head,
                                          uft_track_t *track) {
    d64_pd_t *p = disk->plugin_data;
    if (!p || !p->img || head != 0) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    int d64_track = cyl + 1;  /* D64 is 1-based */
    if (d64_track < 1 || d64_track > p->max_track) return UFT_OK;
    int nsectors = (d64_track <= 40) ? d64_spt[d64_track] : 17;

    uint8_t buf[256];
    for (int s = 0; s < nsectors; s++) {
        if (d64_read_sector(p->img, d64_track, s, buf) == 0) {
            uft_format_add_sector(track, (uint8_t)s, buf, 256,
                                  (uint8_t)cyl, (uint8_t)head);
        }
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_d64 = {
    .name = "D64", .description = "Commodore 1541 D64",
    .extensions = "d64", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = d64_plugin_probe, .open = d64_plugin_open,
    .close = d64_plugin_close, .read_track = d64_plugin_read_track,
};
UFT_REGISTER_FORMAT_PLUGIN(d64)
