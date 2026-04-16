/**
 * @file uft_syn.c
 * @brief SYN (Synclavier) Plugin
 *
 * Synclavier floppy: 77 cyl × 2 heads × 16 spt × 256 = 634880 bytes.
 * Headerless raw format used by Synclavier digital synthesizer.
 */
#include "uft/uft_format_common.h"
typedef struct { FILE* file; } syn_data_t;
bool syn_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)d; (void)s;
    if (fs == 634880) { *c = 35; return true; } return false;
}
static uft_error_t syn_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    syn_data_t *p = calloc(1, sizeof(syn_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f; disk->plugin_data = p;
    disk->geometry.cylinders = 77; disk->geometry.heads = 2;
    disk->geometry.sectors = 16; disk->geometry.sector_size = 256;
    disk->geometry.total_sectors = 77*2*16; return UFT_OK;
}
static void syn_close(uft_disk_t *d) {
    syn_data_t *p = d->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); d->plugin_data = NULL; }
}
static uft_error_t syn_read_track(uft_disk_t *d, int cyl, int head, uft_track_t *t) {
    syn_data_t *p = d->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    uft_track_init(t, cyl, head);
    long off = (long)(((uint32_t)cyl*2+head)*16*256);
    if (fseek(p->file, off, SEEK_SET) != 0) return UFT_ERROR_IO;
    uint8_t buf[256];
    for (int s = 0; s < 16; s++) {
        if (fread(buf, 1, 256, p->file) != 256) return UFT_ERROR_IO;
        uft_format_add_sector(t, (uint8_t)s, buf, 256, (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}
static uft_error_t syn_write_track(uft_disk_t *d, int cyl, int head,
                                    const uft_track_t *t) {
    syn_data_t *p = d->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (d->read_only) return UFT_ERROR_NOT_SUPPORTED;
    long off = (long)(((uint32_t)cyl*2+head)*16*256);
    for (size_t s = 0; s < t->sector_count && (int)s < 16; s++) {
        if (fseek(p->file, off + (long)s * 256, SEEK_SET) != 0) return UFT_ERROR_IO;
        const uint8_t *data = t->sectors[s].data;
        uint8_t pad[256];
        if (!data || t->sectors[s].data_len == 0) { memset(pad, 0xE5, 256); data = pad; }
        if (fwrite(data, 1, 256, p->file) != 256) return UFT_ERROR_IO;
    }
    return UFT_OK;
}
const uft_format_plugin_t uft_format_plugin_syn = {
    .name = "SYN", .description = "Synclavier Disk",
    .extensions = "syn", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = syn_probe, .open = syn_open, .close = syn_close,
    .read_track = syn_read_track, .write_track = syn_write_track,
};
UFT_REGISTER_FORMAT_PLUGIN(syn)
