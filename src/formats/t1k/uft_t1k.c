/**
 * @file uft_t1k.c
 * @brief T1K (Tandy 1000) Plugin — IBM PC compatible raw disk
 *
 * Tandy 1000 used standard IBM PC format: 360K/720K/1.2M/1.44M.
 * Same geometry as IMG but from Tandy-specific tools.
 */
#include "uft/uft_format_common.h"

typedef struct { FILE* file; uint8_t cyl; uint8_t heads; uint8_t spt; } t1k_data_t;

bool t1k_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)d; (void)s;
    if (fs == 368640 || fs == 737280 || fs == 1228800 || fs == 1474560) {
        *c = 25; return true;
    }
    return false;
}

static uft_error_t t1k_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long fs = ftell(f); if (fs < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    t1k_data_t *p = calloc(1, sizeof(t1k_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    switch (fs) {
        case 368640:  p->cyl = 40; p->heads = 2; p->spt = 9; break;
        case 737280:  p->cyl = 80; p->heads = 2; p->spt = 9; break;
        case 1228800: p->cyl = 80; p->heads = 2; p->spt = 15; break;
        case 1474560: p->cyl = 80; p->heads = 2; p->spt = 18; break;
        default: free(p); fclose(f); return UFT_ERROR_FORMAT_INVALID;
    }
    disk->plugin_data = p;
    disk->geometry.cylinders = p->cyl; disk->geometry.heads = p->heads;
    disk->geometry.sectors = p->spt; disk->geometry.sector_size = 512;
    disk->geometry.total_sectors = (uint32_t)p->cyl * p->heads * p->spt;
    return UFT_OK;
}

static void t1k_close(uft_disk_t *d) {
    t1k_data_t *p = d->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); d->plugin_data = NULL; }
}

static uft_error_t t1k_read_track(uft_disk_t *d, int cyl, int head, uft_track_t *t) {
    t1k_data_t *p = d->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    uft_track_init(t, cyl, head);
    long off = (long)(((uint32_t)cyl * p->heads + head) * p->spt * 512);
    if (fseek(p->file, off, SEEK_SET) != 0) return UFT_ERROR_IO;
    uint8_t buf[512];
    for (int s = 0; s < p->spt; s++) {
        if (fread(buf, 1, 512, p->file) != 512) return UFT_ERROR_IO;
        uft_format_add_sector(t, (uint8_t)s, buf, 512, (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_t1k = {
    .name = "T1K", .description = "Tandy 1000 Disk",
    .extensions = "dsk;t1k", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ,
    .probe = t1k_probe, .open = t1k_open, .close = t1k_close,
    .read_track = t1k_read_track,
};
UFT_REGISTER_FORMAT_PLUGIN(t1k)
