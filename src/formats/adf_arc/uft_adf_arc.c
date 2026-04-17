/**
 * @file uft_adf_arc.c
 * @brief ADF_ARC (Acorn Archimedes) Plugin
 *
 * Acorn Archimedes .adf files: headerless raw sector dumps.
 * 800K: 80 cyl × 2 heads × 5 spt × 1024 = 819200
 * 1.6M: 80 cyl × 2 heads × 10 spt × 512 (some variants)
 * ADFS-D: 80 × 2 × 16 × 256 = 655360
 */
#include "uft/uft_format_common.h"

typedef struct { FILE* file; uint8_t cyl; uint8_t heads; uint8_t spt; uint16_t ss; } adf_arc_data_t;

bool adf_arc_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)d; (void)s;
    if (fs == 819200 || fs == 655360 || fs == 327680 || fs == 1638400) {
        *c = 35; return true;
    }
    return false;
}

static uft_error_t adf_arc_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long fs = ftell(f); if (fs < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    adf_arc_data_t *p = calloc(1, sizeof(adf_arc_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    switch (fs) {
        case 819200:  p->cyl=80; p->heads=2; p->spt=5;  p->ss=1024; break;
        case 655360:  p->cyl=80; p->heads=2; p->spt=16; p->ss=256;  break;
        case 327680:  p->cyl=80; p->heads=1; p->spt=16; p->ss=256;  break;
        case 1638400: p->cyl=80; p->heads=2; p->spt=10; p->ss=1024; break;
        default: free(p); fclose(f); return UFT_ERROR_FORMAT_INVALID;
    }
    disk->plugin_data = p;
    disk->geometry.cylinders = p->cyl; disk->geometry.heads = p->heads;
    disk->geometry.sectors = p->spt; disk->geometry.sector_size = p->ss;
    disk->geometry.total_sectors = (uint32_t)p->cyl * p->heads * p->spt;
    return UFT_OK;
}

static void adf_arc_close(uft_disk_t *d) {
    adf_arc_data_t *p = d->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); d->plugin_data = NULL; }
}

static uft_error_t adf_arc_read_track(uft_disk_t *d, int cyl, int head, uft_track_t *t) {
    adf_arc_data_t *p = d->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    uft_track_init(t, cyl, head);
    long off = (long)(((uint32_t)cyl * p->heads + head) * p->spt * p->ss);
    if (fseek(p->file, off, SEEK_SET) != 0) return UFT_ERROR_IO;
    uint8_t buf[1024];
    for (int s = 0; s < p->spt; s++) {
        if (fread(buf, 1, p->ss, p->file) != p->ss) return UFT_ERROR_IO;
        uft_format_add_sector(t, (uint8_t)s, buf, p->ss, (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

static uft_error_t adf_arc_write_track(uft_disk_t *d, int cyl, int head,
                                        const uft_track_t *t) {
    adf_arc_data_t *p = d->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (d->read_only) return UFT_ERROR_NOT_SUPPORTED;
    long off = (long)(((uint32_t)cyl * p->heads + head) * p->spt * p->ss);
    for (size_t s = 0; s < t->sector_count && (int)s < p->spt; s++) {
        if (fseek(p->file, off + (long)s * p->ss, SEEK_SET) != 0) return UFT_ERROR_IO;
        const uint8_t *data = t->sectors[s].data;
        uint8_t pad[1024];
        if (!data || t->sectors[s].data_len == 0) { memset(pad, 0xE5, p->ss); data = pad; }
        if (fwrite(data, 1, p->ss, p->file) != p->ss) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_adf_arc = {
    .name = "ADF_ARC", .description = "Acorn Archimedes ADFS",
    .extensions = "adf;adl;adm", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = adf_arc_probe, .open = adf_arc_open, .close = adf_arc_close,
    .read_track = adf_arc_read_track, .write_track = adf_arc_write_track,
    .verify_track = uft_generic_verify_track,
};
UFT_REGISTER_FORMAT_PLUGIN(adf_arc)
