/**
 * @file uft_tan.c
 * @brief TAN (Tandy TRS-80 Model I/III/4) Plugin
 *
 * Raw sector dump, typically 35-40 tracks × 1-2 sides × 10-18 spt × 256 bytes.
 * Same as JV1 but from Tandy-era tools.
 */
#include "uft/uft_format_common.h"

typedef struct { FILE* file; uint8_t cyl; uint8_t heads; uint8_t spt; uint16_t ss; } tan_data_t;

bool tan_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)d; (void)s;
    if (fs == 89600 || fs == 179200 || fs == 204800 || fs == 102400) {
        *c = 30; return true;
    }
    return false;
}

static uft_error_t tan_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long fs = ftell(f); if (fs < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    tan_data_t *p = calloc(1, sizeof(tan_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f; p->ss = 256; p->spt = 10;
    uint32_t tracks = (uint32_t)fs / (p->spt * p->ss);
    if (tracks <= 40) { p->cyl = (uint8_t)tracks; p->heads = 1; }
    else { p->cyl = (uint8_t)(tracks / 2); p->heads = 2; }
    disk->plugin_data = p;
    disk->geometry.cylinders = p->cyl; disk->geometry.heads = p->heads;
    disk->geometry.sectors = p->spt; disk->geometry.sector_size = p->ss;
    disk->geometry.total_sectors = (uint32_t)p->cyl * p->heads * p->spt;
    return UFT_OK;
}

static void tan_close(uft_disk_t *d) {
    tan_data_t *p = d->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); d->plugin_data = NULL; }
}

static uft_error_t tan_read_track(uft_disk_t *d, int cyl, int head, uft_track_t *t) {
    tan_data_t *p = d->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    uft_track_init(t, cyl, head);
    long off = (long)((head * p->cyl + cyl) * p->spt * p->ss);
    if (fseek(p->file, off, SEEK_SET) != 0) return UFT_ERROR_IO;
    uint8_t buf[256];
    for (int s = 0; s < p->spt; s++) {
        if (fread(buf, 1, p->ss, p->file) != p->ss) return UFT_ERROR_IO;
        uft_format_add_sector(t, (uint8_t)s, buf, p->ss, (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_tan = {
    .name = "TAN", .description = "Tandy TRS-80",
    .extensions = "dsk;trs", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ,
    .probe = tan_probe, .open = tan_open, .close = tan_close,
    .read_track = tan_read_track,
};
UFT_REGISTER_FORMAT_PLUGIN(tan)
