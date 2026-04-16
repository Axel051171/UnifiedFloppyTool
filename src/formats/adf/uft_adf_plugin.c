/**
 * @file uft_adf_plugin.c
 * @brief ADF (Amiga Disk File) Plugin-B — self-contained
 *
 * ADF is a headerless raw sector dump of Amiga floppy disks.
 * DD: 80 cyl × 2 heads × 11 spt × 512 = 901,120 bytes
 * HD: 80 cyl × 2 heads × 22 spt × 512 = 1,802,240 bytes
 */
#include "uft/uft_format_common.h"

#define ADF_DD_SIZE     901120
#define ADF_HD_SIZE     1802240
#define ADF_SECTOR_SIZE 512

typedef struct { FILE *file; uint8_t spt; } adf_pd_t;

static bool adf_plugin_probe(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    if (file_size != ADF_DD_SIZE && file_size != ADF_HD_SIZE) return false;
    *confidence = 70;
    /* Amiga bootblock: "DOS\0" at offset 0 */
    if (size >= 4 && data[0] == 'D' && data[1] == 'O' && data[2] == 'S')
        *confidence = 95;
    return true;
}

static uft_error_t adf_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    uint8_t spt = (sz == ADF_HD_SIZE) ? 22 : 11;
    if (sz != ADF_DD_SIZE && sz != ADF_HD_SIZE) { fclose(f); return UFT_ERROR_FORMAT_INVALID; }

    adf_pd_t *p = calloc(1, sizeof(adf_pd_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f; p->spt = spt;

    disk->plugin_data = p;
    disk->geometry.cylinders = 80;
    disk->geometry.heads = 2;
    disk->geometry.sectors = spt;
    disk->geometry.sector_size = ADF_SECTOR_SIZE;
    disk->geometry.total_sectors = 80 * 2 * spt;
    return UFT_OK;
}

static void adf_close(uft_disk_t *disk) {
    adf_pd_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t adf_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track) {
    adf_pd_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);

    long off = (long)((cyl * 2 + head) * p->spt * ADF_SECTOR_SIZE);
    uint8_t buf[ADF_SECTOR_SIZE];
    for (int s = 0; s < p->spt; s++) {
        if (fseek(p->file, off + s * ADF_SECTOR_SIZE, SEEK_SET) != 0) return UFT_ERROR_IO;
        if (fread(buf, 1, ADF_SECTOR_SIZE, p->file) != ADF_SECTOR_SIZE) {
            memset(buf, 0xE5, ADF_SECTOR_SIZE);
        }
        uft_format_add_sector(track, (uint8_t)s, buf, ADF_SECTOR_SIZE,
                              (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

static uft_error_t adf_write_track(uft_disk_t *disk, int cyl, int head,
                                    const uft_track_t *track) {
    adf_pd_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    long off = (long)((cyl * 2 + head) * p->spt * ADF_SECTOR_SIZE);
    for (size_t s = 0; s < track->sector_count && (int)s < p->spt; s++) {
        if (fseek(p->file, off + (long)s * ADF_SECTOR_SIZE, SEEK_SET) != 0) return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[ADF_SECTOR_SIZE];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, ADF_SECTOR_SIZE); data = pad;
        }
        if (fwrite(data, 1, ADF_SECTOR_SIZE, p->file) != ADF_SECTOR_SIZE) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_adf = {
    .name = "ADF", .description = "Amiga Disk File",
    .extensions = "adf", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = adf_plugin_probe, .open = adf_open,
    .close = adf_close, .read_track = adf_read_track,
    .write_track = adf_write_track,
};
UFT_REGISTER_FORMAT_PLUGIN(adf)
