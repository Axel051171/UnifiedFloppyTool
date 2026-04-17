/**
 * @file uft_xdm86.c
 * @brief XDM86 (TI-99/4A Disk Manager) Plugin
 *
 * XDM86-format TI-99 disk images. Standard geometry:
 * 40 cyl × 1-2 heads × 9 spt × 256 bytes.
 * SS/SD: 92160, DS/SD: 184320, DS/DD: 368640.
 */
#include "uft/uft_format_common.h"

typedef struct { FILE* file; uint8_t cyl; uint8_t heads; uint8_t spt; } xdm_data_t;

bool xdm86_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)d; (void)s;
    if (fs == 92160 || fs == 184320 || fs == 368640) { *c = 35; return true; }
    return false;
}

static uft_error_t xdm86_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long fs = ftell(f); if (fs < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    xdm_data_t *p = calloc(1, sizeof(xdm_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    switch (fs) {
        case 92160:  p->cyl=40; p->heads=1; p->spt=9; break;
        case 184320: p->cyl=40; p->heads=2; p->spt=9; break;
        case 368640: p->cyl=40; p->heads=2; p->spt=18; break;
        default: free(p); fclose(f); return UFT_ERROR_FORMAT_INVALID;
    }
    disk->plugin_data = p;
    disk->geometry.cylinders = p->cyl; disk->geometry.heads = p->heads;
    disk->geometry.sectors = p->spt; disk->geometry.sector_size = 256;
    disk->geometry.total_sectors = (uint32_t)p->cyl * p->heads * p->spt;
    return UFT_OK;
}

static void xdm86_close(uft_disk_t *d) {
    xdm_data_t *p = d->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); d->plugin_data = NULL; }
}

static uft_error_t xdm86_read_track(uft_disk_t *d, int cyl, int head, uft_track_t *t) {
    xdm_data_t *p = d->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    uft_track_init(t, cyl, head);
    long off = (long)(((uint32_t)cyl * p->heads + head) * p->spt * 256);
    if (fseek(p->file, off, SEEK_SET) != 0) return UFT_ERROR_IO;
    uint8_t buf[256];
    for (int s = 0; s < p->spt; s++) {
        if (fread(buf, 1, 256, p->file) != 256) return UFT_ERROR_IO;
        uft_format_add_sector(t, (uint8_t)s, buf, 256, (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

static uft_error_t xdm86_write_track(uft_disk_t *d, int cyl, int head,
                                      const uft_track_t *t) {
    xdm_data_t *p = d->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (d->read_only) return UFT_ERROR_NOT_SUPPORTED;
    long off = (long)(((uint32_t)cyl * p->heads + head) * p->spt * 256);
    for (size_t s = 0; s < t->sector_count && (int)s < p->spt; s++) {
        if (fseek(p->file, off + (long)s * 256, SEEK_SET) != 0) return UFT_ERROR_IO;
        const uint8_t *data = t->sectors[s].data;
        uint8_t pad[256];
        if (!data || t->sectors[s].data_len == 0) { memset(pad, 0xE5, 256); data = pad; }
        if (fwrite(data, 1, 256, p->file) != 256) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_xdm86 = {
    .name = "XDM86", .description = "TI-99/4A Disk Manager",
    .extensions = "dsk;v9t9", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = xdm86_probe, .open = xdm86_open, .close = xdm86_close,
    .read_track = xdm86_read_track, .write_track = xdm86_write_track,
    .verify_track = uft_generic_verify_track,
};
UFT_REGISTER_FORMAT_PLUGIN(xdm86)
