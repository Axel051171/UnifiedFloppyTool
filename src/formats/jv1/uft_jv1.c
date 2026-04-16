/**
 * @file uft_jv1.c
 * @brief JV1 (TRS-80) Disk Image Plugin
 *
 * JV1 is the simplest TRS-80 disk image format — headerless raw sectors.
 * Used by Jeff Vavasour's TRS-80 emulator and widely in the TRS-80 community.
 *
 * Layout: raw 256-byte sectors, 10 sectors/track, sequential.
 * Geometry determined by file size:
 *   SS/SD 35 track:  35 × 10 × 256 =  89,600
 *   SS/SD 40 track:  40 × 10 × 256 = 102,400
 *   DS/SD 35 track:  35 × 10 × 256 × 2 = 179,200
 *   DS/SD 40 track:  40 × 10 × 256 × 2 = 204,800
 *
 * Track interleave: side 0 tracks first, then side 1.
 *
 * Reference: Tim Mann's TRS-80 documentation
 */

#include "uft/uft_format_common.h"

#define JV1_SECTOR_SIZE     256
#define JV1_SPT             10

typedef struct {
    FILE*       file;
    uint8_t     cylinders;
    uint8_t     heads;
} jv1_data_t;

static bool jv1_detect_geometry(size_t file_size,
                                uint8_t *cyl, uint8_t *heads)
{
    if (file_size == 0 || file_size % (JV1_SPT * JV1_SECTOR_SIZE) != 0)
        return false;

    uint32_t total_tracks = (uint32_t)(file_size / (JV1_SPT * JV1_SECTOR_SIZE));

    if (total_tracks <= 40) {
        *cyl = (uint8_t)total_tracks;
        *heads = 1;
        return true;
    }
    if (total_tracks <= 80 && total_tracks % 2 == 0) {
        *cyl = (uint8_t)(total_tracks / 2);
        *heads = 2;
        return true;
    }
    return false;
}

bool jv1_probe(const uint8_t *data, size_t size, size_t file_size,
               int *confidence)
{
    (void)data; (void)size;
    uint8_t cyl, heads;
    if (!jv1_detect_geometry(file_size, &cyl, &heads)) return false;
    *confidence = 35;  /* Low — no magic, size-only detection */
    return true;
}

static uft_error_t jv1_open(uft_disk_t *disk, const char *path,
                             bool read_only)
{
    FILE *f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long fs = ftell(f);
    if (fs < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    uint8_t cyl, heads;
    if (!jv1_detect_geometry((size_t)fs, &cyl, &heads)) {
        fclose(f); return UFT_ERROR_FORMAT_INVALID;
    }

    jv1_data_t *pdata = calloc(1, sizeof(jv1_data_t));
    if (!pdata) { fclose(f); return UFT_ERROR_NO_MEMORY; }

    pdata->file = f;
    pdata->cylinders = cyl;
    pdata->heads = heads;

    disk->plugin_data = pdata;
    disk->geometry.cylinders = cyl;
    disk->geometry.heads = heads;
    disk->geometry.sectors = JV1_SPT;
    disk->geometry.sector_size = JV1_SECTOR_SIZE;
    disk->geometry.total_sectors = (uint32_t)cyl * heads * JV1_SPT;
    return UFT_OK;
}

static void jv1_close(uft_disk_t *disk)
{
    jv1_data_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t jv1_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track)
{
    jv1_data_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (cyl >= p->cylinders || head >= p->heads) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    /* Side 0 first, then side 1 */
    long offset = (long)((head * p->cylinders + cyl) * JV1_SPT * JV1_SECTOR_SIZE);
    if (fseek(p->file, offset, SEEK_SET) != 0) return UFT_ERROR_IO;

    uint8_t buf[JV1_SECTOR_SIZE];
    for (int s = 0; s < JV1_SPT; s++) {
        if (fread(buf, 1, JV1_SECTOR_SIZE, p->file) != JV1_SECTOR_SIZE)
            return UFT_ERROR_IO;
        uft_format_add_sector(track, (uint8_t)s, buf, JV1_SECTOR_SIZE,
                              (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

static uft_error_t jv1_write_track(uft_disk_t *disk, int cyl, int head,
                                    const uft_track_t *track) {
    jv1_data_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    long offset = (long)((head * p->cylinders + cyl) * JV1_SPT * JV1_SECTOR_SIZE);
    for (size_t s = 0; s < track->sector_count && (int)s < JV1_SPT; s++) {
        if (fseek(p->file, offset + (long)s * JV1_SECTOR_SIZE, SEEK_SET) != 0)
            return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[JV1_SECTOR_SIZE];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, JV1_SECTOR_SIZE); data = pad;
        }
        if (fwrite(data, 1, JV1_SECTOR_SIZE, p->file) != JV1_SECTOR_SIZE)
            return UFT_ERROR_IO;
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_jv1 = {
    .name = "JV1", .description = "TRS-80 JV1 (Jeff Vavasour)",
    .extensions = "jv1;dsk", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = jv1_probe, .open = jv1_open, .close = jv1_close,
    .read_track = jv1_read_track, .write_track = jv1_write_track,
};
UFT_REGISTER_FORMAT_PLUGIN(jv1)
