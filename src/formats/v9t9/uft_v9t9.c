/**
 * @file uft_v9t9.c
 * @brief V9T9 (TI-99/4A) Plugin-B
 *
 * V9T9 = headerless raw TI-99/4A disk. 40 tracks × 1-2 heads × 9 spt × 256.
 * SS/SD: 92160, DS/SD: 184320, DS/DD: 368640.
 */
#include "uft/uft_format_common.h"

typedef struct { FILE *file; uint8_t cyl, heads, spt; } v9t9_pd_t;

static bool v9t9_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)d; (void)s;
    if (fs == 92160 || fs == 184320 || fs == 368640) { *c = 40; return true; }
    return false;
}

static uft_error_t v9t9_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long sz = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    v9t9_pd_t *p = calloc(1, sizeof(v9t9_pd_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f; p->cyl = 40;
    switch (sz) {
        case 92160:  p->heads = 1; p->spt = 9; break;
        case 184320: p->heads = 2; p->spt = 9; break;
        case 368640: p->heads = 2; p->spt = 18; break;
        default: free(p); fclose(f); return UFT_ERROR_FORMAT_INVALID;
    }
    disk->plugin_data = p;
    disk->geometry.cylinders = p->cyl; disk->geometry.heads = p->heads;
    disk->geometry.sectors = p->spt; disk->geometry.sector_size = 256;
    disk->geometry.total_sectors = (uint32_t)p->cyl * p->heads * p->spt;
    return UFT_OK;
}

static void v9t9_close(uft_disk_t *disk) {
    v9t9_pd_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t v9t9_read_track(uft_disk_t *disk, int cyl, int head, uft_track_t *track) {
    v9t9_pd_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);
    long off = (long)((cyl * p->heads + head) * p->spt * 256);
    uint8_t buf[256];
    for (int s = 0; s < p->spt; s++) {
        if (fseek(p->file, off + s * 256, SEEK_SET) != 0) return UFT_ERROR_IO;
        if (fread(buf, 1, 256, p->file) != 256) { memset(buf, 0xE5, 256); }
        uft_format_add_sector(track, (uint8_t)s, buf, 256, (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

static uft_error_t v9t9_write_track(uft_disk_t *disk, int cyl, int head,
                                     const uft_track_t *track) {
    v9t9_pd_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    long off = (long)((cyl * p->heads + head) * p->spt * 256);
    for (size_t s = 0; s < track->sector_count && (int)s < p->spt; s++) {
        if (fseek(p->file, off + (long)s * 256, SEEK_SET) != 0) return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[256];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, 256); data = pad;
        }
        if (fwrite(data, 1, 256, p->file) != 256) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_v9t9 = {
    .name = "V9T9", .description = "TI-99/4A V9T9",
    .extensions = "v9t9;dsk", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = v9t9_probe, .open = v9t9_open, .close = v9t9_close,
    .read_track = v9t9_read_track, .write_track = v9t9_write_track,
    .verify_track = uft_generic_verify_track,
};
UFT_REGISTER_FORMAT_PLUGIN(v9t9)
