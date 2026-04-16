/**
 * @file uft_po.c
 * @brief PO (Apple II ProDOS Order) Plugin-B
 *
 * Same as DO but ProDOS sector order (non-interleaved).
 * 35 tracks × 16 sectors × 256 bytes = 143,360 bytes.
 */
#include "uft/uft_format_common.h"

#define PO_SIZE   143360
#define PO_TRACKS 35
#define PO_SPT    16
#define PO_SS     256

typedef struct { FILE *file; } po_pd_t;

static bool po_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)d; (void)s;
    if (fs == PO_SIZE) { *c = 55; return true; }
    return false;
}

static uft_error_t po_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    po_pd_t *p = calloc(1, sizeof(po_pd_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    disk->plugin_data = p;
    disk->geometry.cylinders = PO_TRACKS; disk->geometry.heads = 1;
    disk->geometry.sectors = PO_SPT; disk->geometry.sector_size = PO_SS;
    disk->geometry.total_sectors = PO_TRACKS * PO_SPT;
    return UFT_OK;
}

static void po_close(uft_disk_t *disk) {
    po_pd_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t po_read_track(uft_disk_t *disk, int cyl, int head, uft_track_t *track) {
    po_pd_t *p = disk->plugin_data;
    if (!p || !p->file || head != 0) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);
    uint8_t buf[PO_SS];
    for (int s = 0; s < PO_SPT; s++) {
        if (fseek(p->file, (long)(cyl * PO_SPT + s) * PO_SS, SEEK_SET) != 0) return UFT_ERROR_IO;
        if (fread(buf, 1, PO_SS, p->file) != PO_SS) { memset(buf, 0xE5, PO_SS); }
        uft_format_add_sector(track, (uint8_t)s, buf, PO_SS, (uint8_t)cyl, 0);
    }
    return UFT_OK;
}

static uft_error_t po_write_track(uft_disk_t *disk, int cyl, int head,
                                   const uft_track_t *track) {
    po_pd_t *p = disk->plugin_data;
    if (!p || !p->file || head != 0) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    for (size_t s = 0; s < track->sector_count && (int)s < PO_SPT; s++) {
        if (fseek(p->file, (long)(cyl * PO_SPT + (int)s) * PO_SS, SEEK_SET) != 0)
            return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[PO_SS];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, PO_SS); data = pad;
        }
        if (fwrite(data, 1, PO_SS, p->file) != PO_SS) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_po = {
    .name = "PO", .description = "Apple II ProDOS Order",
    .extensions = "po;dsk", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = po_probe, .open = po_open, .close = po_close,
    .read_track = po_read_track, .write_track = po_write_track,
};
UFT_REGISTER_FORMAT_PLUGIN(po)
