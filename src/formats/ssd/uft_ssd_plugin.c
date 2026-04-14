/**
 * @file uft_ssd_plugin.c
 * @brief SSD/DSD (BBC Micro) Plugin
 *
 * SSD = Single-Sided Disk image, DSD = Double-Sided.
 * Headerless raw 256-byte sectors, 10 sectors/track.
 *
 * Geometry:
 *   SSD: 80 × 1 × 10 × 256 = 204,800 (or 40 tracks = 102,400)
 *   DSD: 80 × 2 × 10 × 256 = 409,600 (or 40 tracks = 204,800)
 *
 * DSD interleave: track 0 side 0, track 0 side 1, track 1 side 0, ...
 *
 * Reference: BeebWiki SSD/DSD format
 */

#include "uft/uft_format_common.h"

#define SSD_SECTOR_SIZE     256
#define SSD_SPT             10

typedef struct {
    FILE*       file;
    uint8_t     cylinders;
    uint8_t     heads;
    bool        interleaved;    /* DSD: sides interleaved per track */
} ssd_data_t;

static bool ssd_detect(size_t file_size, uint8_t *cyl, uint8_t *heads)
{
    uint32_t track_size = SSD_SPT * SSD_SECTOR_SIZE;
    if (file_size == 0 || file_size % track_size != 0) return false;

    uint32_t total = (uint32_t)(file_size / track_size);
    if (total == 40)  { *cyl = 40; *heads = 1; return true; }
    if (total == 80)  { *cyl = 80; *heads = 1; return true; }
    if (total == 160) { *cyl = 80; *heads = 2; return true; }
    return false;
}

bool ssd_probe(const uint8_t *data, size_t size, size_t file_size,
               int *confidence)
{
    (void)data; (void)size;
    uint8_t cyl, heads;
    if (!ssd_detect(file_size, &cyl, &heads)) return false;
    *confidence = 30;
    return true;
}

static uft_error_t ssd_open(uft_disk_t *disk, const char *path, bool ro)
{
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long fs = ftell(f);
    if (fs < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    uint8_t cyl, heads;
    if (!ssd_detect((size_t)fs, &cyl, &heads)) { fclose(f); return UFT_ERROR_FORMAT_INVALID; }

    ssd_data_t *p = calloc(1, sizeof(ssd_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f; p->cylinders = cyl; p->heads = heads;
    p->interleaved = (heads == 2);

    disk->plugin_data = p;
    disk->geometry.cylinders = cyl;
    disk->geometry.heads = heads;
    disk->geometry.sectors = SSD_SPT;
    disk->geometry.sector_size = SSD_SECTOR_SIZE;
    disk->geometry.total_sectors = (uint32_t)cyl * heads * SSD_SPT;
    return UFT_OK;
}

static void ssd_close(uft_disk_t *disk)
{
    ssd_data_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t ssd_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track)
{
    ssd_data_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    long offset;
    if (p->interleaved) {
        /* DSD: cyl0/side0, cyl0/side1, cyl1/side0, ... */
        offset = (long)((cyl * 2 + head) * SSD_SPT * SSD_SECTOR_SIZE);
    } else {
        offset = (long)(cyl * SSD_SPT * SSD_SECTOR_SIZE);
    }

    if (fseek(p->file, offset, SEEK_SET) != 0) return UFT_ERROR_IO;

    uint8_t buf[SSD_SECTOR_SIZE];
    for (int s = 0; s < SSD_SPT; s++) {
        if (fread(buf, 1, SSD_SECTOR_SIZE, p->file) != SSD_SECTOR_SIZE)
            return UFT_ERROR_IO;
        uft_format_add_sector(track, (uint8_t)s, buf, SSD_SECTOR_SIZE,
                              (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_ssd = {
    .name = "SSD", .description = "BBC Micro SSD/DSD",
    .extensions = "ssd;dsd", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ,
    .probe = ssd_probe, .open = ssd_open, .close = ssd_close,
    .read_track = ssd_read_track,
};
UFT_REGISTER_FORMAT_PLUGIN(ssd)
