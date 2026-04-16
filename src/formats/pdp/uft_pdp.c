/**
 * @file uft_pdp.c
 * @brief PDP (DEC PDP-11/VAX) Plugin
 *
 * DEC RX01/RX02 floppy images. RX01: 77 cyl × 1 head × 26 spt × 128.
 * RX02: 77 × 1 × 26 × 256. Total: 256256 (RX01) or 512512 (RX02).
 */
#include "uft/uft_format_common.h"

typedef struct { FILE* file; uint16_t ss; } pdp_data_t;

bool pdp_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)d; (void)s;
    if (fs == 256256 || fs == 512512) { *c = 40; return true; }
    return false;
}

static uft_error_t pdp_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long fs = ftell(f); if (fs < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    pdp_data_t *p = calloc(1, sizeof(pdp_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    p->ss = (fs == 512512) ? 256 : 128;
    disk->plugin_data = p;
    disk->geometry.cylinders = 77; disk->geometry.heads = 1;
    disk->geometry.sectors = 26; disk->geometry.sector_size = p->ss;
    disk->geometry.total_sectors = 77 * 26;
    return UFT_OK;
}

static void pdp_close(uft_disk_t *d) {
    pdp_data_t *p = d->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); d->plugin_data = NULL; }
}

static uft_error_t pdp_read_track(uft_disk_t *d, int cyl, int head, uft_track_t *t) {
    pdp_data_t *p = d->plugin_data;
    if (!p || !p->file || head != 0) return UFT_ERROR_INVALID_STATE;
    uft_track_init(t, cyl, head);
    long off = (long)(cyl * 26 * p->ss);
    if (fseek(p->file, off, SEEK_SET) != 0) return UFT_ERROR_IO;
    uint8_t buf[256];
    for (int s = 0; s < 26; s++) {
        if (fread(buf, 1, p->ss, p->file) != p->ss) return UFT_ERROR_IO;
        uft_format_add_sector(t, (uint8_t)s, buf, p->ss, (uint8_t)cyl, 0);
    }
    return UFT_OK;
}

static uft_error_t pdp_write_track(uft_disk_t *d, int cyl, int head,
                                    const uft_track_t *t) {
    pdp_data_t *p = d->plugin_data;
    if (!p || !p->file || head != 0) return UFT_ERROR_INVALID_STATE;
    if (d->read_only) return UFT_ERROR_NOT_SUPPORTED;
    long off = (long)(cyl * 26 * p->ss);
    for (size_t s = 0; s < t->sector_count && (int)s < 26; s++) {
        if (fseek(p->file, off + (long)s * p->ss, SEEK_SET) != 0) return UFT_ERROR_IO;
        const uint8_t *data = t->sectors[s].data;
        uint8_t pad[256];
        if (!data || t->sectors[s].data_len == 0) { memset(pad, 0xE5, p->ss); data = pad; }
        if (fwrite(data, 1, p->ss, p->file) != p->ss) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_pdp = {
    .name = "PDP", .description = "DEC PDP-11 RX01/RX02",
    .extensions = "rx;rx01;rx02;dsk", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = pdp_probe, .open = pdp_open, .close = pdp_close,
    .read_track = pdp_read_track, .write_track = pdp_write_track,
};
UFT_REGISTER_FORMAT_PLUGIN(pdp)
