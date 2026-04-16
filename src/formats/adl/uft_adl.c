/**
 * @file uft_adl.c
 * @brief ADL (Acorn DFS Large) Plugin-B
 *
 * ADL = Acorn DFS disk with 80 tracks (vs 40 for ADFS-S).
 * Headerless, 80 tracks × 1 head × 16 sectors × 256 bytes = 327,680.
 */
#include "uft/uft_format_common.h"

#define ADL_SIZE  327680
#define ADL_CYL   80
#define ADL_SPT   16
#define ADL_SS    256

typedef struct { FILE *file; } adl_pd_t;

static bool adl_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)d; (void)s;
    if (fs == ADL_SIZE) { *c = 50; return true; }
    return false;
}

static uft_error_t adl_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    adl_pd_t *p = calloc(1, sizeof(adl_pd_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    disk->plugin_data = p;
    disk->geometry.cylinders = ADL_CYL; disk->geometry.heads = 1;
    disk->geometry.sectors = ADL_SPT; disk->geometry.sector_size = ADL_SS;
    disk->geometry.total_sectors = ADL_CYL * ADL_SPT;
    return UFT_OK;
}

static void adl_close(uft_disk_t *disk) {
    adl_pd_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t adl_read_track(uft_disk_t *disk, int cyl, int head, uft_track_t *track) {
    adl_pd_t *p = disk->plugin_data;
    if (!p || !p->file || head != 0) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);
    uint8_t buf[ADL_SS];
    for (int s = 0; s < ADL_SPT; s++) {
        if (fseek(p->file, (long)(cyl * ADL_SPT + s) * ADL_SS, SEEK_SET) != 0) return UFT_ERROR_IO;
        if (fread(buf, 1, ADL_SS, p->file) != ADL_SS) { memset(buf, 0xE5, ADL_SS); }
        uft_format_add_sector(track, (uint8_t)s, buf, ADL_SS, (uint8_t)cyl, 0);
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_adl = {
    .name = "ADL", .description = "Acorn DFS Large (80-track)",
    .extensions = "adl;adf", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ,
    .probe = adl_probe, .open = adl_open, .close = adl_close, .read_track = adl_read_track,
};
UFT_REGISTER_FORMAT_PLUGIN(adl)
