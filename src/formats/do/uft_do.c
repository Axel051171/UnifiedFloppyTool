/**
 * @file uft_do.c
 * @brief DO (Apple II DOS 3.3 Order) Plugin-B
 *
 * Headerless raw 140K Apple II disk. 35 tracks × 16 sectors × 256 bytes.
 * Sector order: DOS 3.3 (interleaved).
 */
#include "uft/uft_format_common.h"

#define DO_SIZE     143360
#define DO_TRACKS   35
#define DO_SPT      16
#define DO_SS       256

typedef struct { FILE *file; } do_pd_t;

static bool do_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)d; (void)s;
    if (fs == DO_SIZE) { *c = 60; return true; }
    return false;
}

static uft_error_t do_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    do_pd_t *p = calloc(1, sizeof(do_pd_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    disk->plugin_data = p;
    disk->geometry.cylinders = DO_TRACKS; disk->geometry.heads = 1;
    disk->geometry.sectors = DO_SPT; disk->geometry.sector_size = DO_SS;
    disk->geometry.total_sectors = DO_TRACKS * DO_SPT;
    return UFT_OK;
}

static void do_close(uft_disk_t *disk) {
    do_pd_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t do_read_track(uft_disk_t *disk, int cyl, int head,
                                  uft_track_t *track) {
    do_pd_t *p = disk->plugin_data;
    if (!p || !p->file || head != 0) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);
    uint8_t buf[DO_SS];
    for (int s = 0; s < DO_SPT; s++) {
        long off = (long)(cyl * DO_SPT + s) * DO_SS;
        if (fseek(p->file, off, SEEK_SET) != 0) return UFT_ERROR_IO;
        if (fread(buf, 1, DO_SS, p->file) != DO_SS) { memset(buf, 0xE5, DO_SS); }
        uft_format_add_sector(track, (uint8_t)s, buf, DO_SS, (uint8_t)cyl, 0);
    }
    return UFT_OK;
}

static uft_error_t do_write_track(uft_disk_t *disk, int cyl, int head,
                                   const uft_track_t *track) {
    do_pd_t *p = disk->plugin_data;
    if (!p || !p->file || head != 0) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    for (size_t s = 0; s < track->sector_count && (int)s < DO_SPT; s++) {
        long off = (long)(cyl * DO_SPT + (int)s) * DO_SS;
        if (fseek(p->file, off, SEEK_SET) != 0) return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[DO_SS];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, DO_SS); data = pad;
        }
        if (fwrite(data, 1, DO_SS, p->file) != DO_SS) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_do = {
    .name = "DO", .description = "Apple II DOS 3.3",
    .extensions = "do;dsk", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = do_probe, .open = do_open, .close = do_close,
    .read_track = do_read_track, .write_track = do_write_track,
    .verify_track = uft_generic_verify_track,
};
UFT_REGISTER_FORMAT_PLUGIN(do)
