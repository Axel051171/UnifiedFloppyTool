/**
 * @file uft_d13.c
 * @brief D13 (Apple II DOS 3.2, 13-sector) Plugin-B
 *
 * Headerless raw 113.75K Apple II disk image.
 * 35 tracks x 13 sectors x 256 bytes = 116,480 bytes.
 *
 * This is the 13-sector format used by Apple DOS 3.2 (and earlier),
 * before the 16-sector DOS 3.3 format became standard.
 *
 * Reference: Beneath Apple DOS, Apple II DOS 3.2 manual
 */

#include "uft/uft_format_common.h"

#define D13_SIZE     116480
#define D13_TRACKS   35
#define D13_SPT      13
#define D13_SS       256

typedef struct {
    FILE *file;
} d13_pd_t;

static bool d13_probe(const uint8_t *d, size_t s, size_t fs, int *c)
{
    (void)d; (void)s;
    if (fs == D13_SIZE) {
        *c = 75;
        return true;
    }
    return false;
}

static uft_error_t d13_open(uft_disk_t *disk, const char *path, bool ro)
{
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;

    /* Verify file size */
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long fs = ftell(f);
    if (fs < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    if ((size_t)fs != D13_SIZE) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }

    d13_pd_t *p = calloc(1, sizeof(d13_pd_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;

    disk->plugin_data = p;
    disk->geometry.cylinders = D13_TRACKS;
    disk->geometry.heads = 1;
    disk->geometry.sectors = D13_SPT;
    disk->geometry.sector_size = D13_SS;
    disk->geometry.total_sectors = D13_TRACKS * D13_SPT;
    return UFT_OK;
}

static void d13_close(uft_disk_t *disk)
{
    d13_pd_t *p = disk->plugin_data;
    if (p) {
        if (p->file) fclose(p->file);
        free(p);
        disk->plugin_data = NULL;
    }
}

static uft_error_t d13_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track)
{
    d13_pd_t *p = disk->plugin_data;
    if (!p || !p->file || head != 0) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    uint8_t buf[D13_SS];
    for (int s = 0; s < D13_SPT; s++) {
        long off = (long)(cyl * D13_SPT + s) * D13_SS;
        if (fseek(p->file, off, SEEK_SET) != 0) return UFT_ERROR_IO;
        if (fread(buf, 1, D13_SS, p->file) != D13_SS) {
            memset(buf, 0xE5, D13_SS);
        }
        uft_format_add_sector(track, (uint8_t)s, buf, D13_SS,
                              (uint8_t)cyl, 0);
    }
    return UFT_OK;
}

static uft_error_t d13_write_track(uft_disk_t *disk, int cyl, int head,
                                    const uft_track_t *track)
{
    d13_pd_t *p = disk->plugin_data;
    if (!p || !p->file || head != 0) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    for (size_t s = 0; s < track->sector_count && (int)s < D13_SPT; s++) {
        long off = (long)(cyl * D13_SPT + (int)s) * D13_SS;
        if (fseek(p->file, off, SEEK_SET) != 0) return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[D13_SS];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, D13_SS);
            data = pad;
        }
        if (fwrite(data, 1, D13_SS, p->file) != D13_SS)
            return UFT_ERROR_IO;
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_d13 = {
    .name = "D13",
    .description = "Apple II DOS 3.2 (13-sector)",
    .extensions = "d13",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = d13_probe,
    .open = d13_open,
    .close = d13_close,
    .read_track = d13_read_track,
    .write_track = d13_write_track,
};
UFT_REGISTER_FORMAT_PLUGIN(d13)
