/**
 * @file uft_sam.c
 * @brief SAM (MGT SAM Coupe) Plugin
 *
 * SAM Coupe used MGT-compatible disk format: 80 cyl × 2 heads × 10 spt × 512.
 * Total: 819,200 bytes. Headerless raw sector dump.
 */
#include "uft/uft_format_common.h"

#define SAM_CYL  80
#define SAM_HEAD 2
#define SAM_SPT  10
#define SAM_SS   512
#define SAM_SIZE (SAM_CYL * SAM_HEAD * SAM_SPT * SAM_SS)

typedef struct { FILE* file; } sam_data_t;

bool sam_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)d; (void)s;
    if (fs == SAM_SIZE) { *c = 35; return true; }
    return false;
}

static uft_error_t sam_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    sam_data_t *p = calloc(1, sizeof(sam_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    disk->plugin_data = p;
    disk->geometry.cylinders = SAM_CYL; disk->geometry.heads = SAM_HEAD;
    disk->geometry.sectors = SAM_SPT; disk->geometry.sector_size = SAM_SS;
    disk->geometry.total_sectors = SAM_CYL * SAM_HEAD * SAM_SPT;
    return UFT_OK;
}

static void sam_close(uft_disk_t *d) {
    sam_data_t *p = d->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); d->plugin_data = NULL; }
}

static uft_error_t sam_read_track(uft_disk_t *d, int cyl, int head, uft_track_t *t) {
    sam_data_t *p = d->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    uft_track_init(t, cyl, head);
    long off = (long)(((uint32_t)cyl * SAM_HEAD + head) * SAM_SPT * SAM_SS);
    if (fseek(p->file, off, SEEK_SET) != 0) return UFT_ERROR_IO;
    uint8_t buf[SAM_SS];
    for (int s = 0; s < SAM_SPT; s++) {
        if (fread(buf, 1, SAM_SS, p->file) != SAM_SS) return UFT_ERROR_IO;
        uft_format_add_sector(t, (uint8_t)s, buf, SAM_SS, (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

static uft_error_t sam_write_track(uft_disk_t *d, int cyl, int head,
                                    const uft_track_t *t) {
    sam_data_t *p = d->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (d->read_only) return UFT_ERROR_NOT_SUPPORTED;
    long off = (long)(((uint32_t)cyl * SAM_HEAD + head) * SAM_SPT * SAM_SS);
    for (size_t s = 0; s < t->sector_count && (int)s < SAM_SPT; s++) {
        if (fseek(p->file, off + (long)s * SAM_SS, SEEK_SET) != 0) return UFT_ERROR_IO;
        const uint8_t *data = t->sectors[s].data;
        uint8_t pad[SAM_SS];
        if (!data || t->sectors[s].data_len == 0) { memset(pad, 0xE5, SAM_SS); data = pad; }
        if (fwrite(data, 1, SAM_SS, p->file) != SAM_SS) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_sam = {
    .name = "SAM", .description = "MGT SAM Coupe",
    .extensions = "mgt;sad;sdf", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = sam_probe, .open = sam_open, .close = sam_close,
    .read_track = sam_read_track, .write_track = sam_write_track,
    .verify_track = uft_generic_verify_track,
};
UFT_REGISTER_FORMAT_PLUGIN(sam)
