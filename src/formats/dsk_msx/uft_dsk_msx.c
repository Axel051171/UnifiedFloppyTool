/**
 * @file uft_dsk_msx.c
 * @brief MSX Disk Image Plugin-B
 *
 * MSX uses IBM-PC compatible FAT12 format (headerless raw sector dump).
 * Common sizes:
 *   720K: 80 cyl x 2 heads x 9 spt x 512 = 737280
 *   360K: 40 cyl x 2 heads x 9 spt x 512 = 368640
 *   360K SS: 40 cyl x 1 head  x 9 spt x 512 = 184320
 *
 * Probe identifies MSX by size + boot sector signature:
 *   Byte 0: 0xEB (JMP) or 0xE9 (JMP)
 *   Bytes 3-10: OEM string (may contain "MSX")
 *   Bytes 11-12: sector size (512 LE)
 *
 * Reference: MSX-DOS specification, openMSX source
 */
#include "uft/uft_format_common.h"

#define MSX_SS 512

typedef struct {
    FILE    *file;
    uint8_t  cyl;
    uint8_t  heads;
    uint8_t  spt;
} msx_pd_t;

static bool msx_detect(size_t file_size, uint8_t *cyl, uint8_t *heads,
                        uint8_t *spt)
{
    *spt = 9;
    switch (file_size) {
        case 737280:  *cyl = 80; *heads = 2; return true;
        case 368640:  *cyl = 40; *heads = 2; return true;
        case 184320:  *cyl = 40; *heads = 1; return true;
        default: return false;
    }
}

static bool msx_probe(const uint8_t *data, size_t size, size_t file_size,
                       int *confidence)
{
    uint8_t cyl, heads, spt;
    if (!msx_detect(file_size, &cyl, &heads, &spt)) return false;

    *confidence = 75;

    /* Check MSX boot sector */
    if (size >= 16) {
        uint8_t b0 = data[0];
        if (b0 == 0xEB || b0 == 0xE9) {
            /* Check sector size in BPB (bytes 11-12, little-endian) */
            uint16_t ss = (uint16_t)data[11] | ((uint16_t)data[12] << 8);
            if (ss == 512) {
                *confidence = 85;
                /* Check for MSX in OEM string (bytes 3-10) */
                if (size >= 11) {
                    bool msx_oem = false;
                    for (int i = 3; i <= 8; i++) {
                        if (data[i] == 'M' && i + 2 <= 10 &&
                            data[i + 1] == 'S' && data[i + 2] == 'X') {
                            msx_oem = true;
                            break;
                        }
                    }
                    if (msx_oem) *confidence = 90;
                }
            }
        }
        /* MSX-DOS alternative: some disks start with 0xEB 0xFE 0x90 */
        if (b0 == 0xEB && size >= 3 && data[1] == 0xFE && data[2] == 0x90)
            *confidence = 90;
    }
    return true;
}

static uft_error_t msx_open(uft_disk_t *disk, const char *path, bool ro)
{
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long fs = ftell(f);
    if (fs < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    uint8_t cyl, heads, spt;
    if (!msx_detect((size_t)fs, &cyl, &heads, &spt)) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }

    msx_pd_t *p = calloc(1, sizeof(msx_pd_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    p->cyl = cyl;
    p->heads = heads;
    p->spt = spt;

    disk->plugin_data = p;
    disk->geometry.cylinders = cyl;
    disk->geometry.heads = heads;
    disk->geometry.sectors = spt;
    disk->geometry.sector_size = MSX_SS;
    disk->geometry.total_sectors = (uint32_t)cyl * heads * spt;
    return UFT_OK;
}

static void msx_close(uft_disk_t *disk)
{
    msx_pd_t *p = disk->plugin_data;
    if (p) {
        if (p->file) fclose(p->file);
        free(p);
        disk->plugin_data = NULL;
    }
}

static uft_error_t msx_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track)
{
    msx_pd_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    long base = (long)((cyl * p->heads + head) * p->spt * MSX_SS);
    uint8_t buf[MSX_SS];

    for (int s = 0; s < p->spt; s++) {
        long off = base + (long)s * MSX_SS;
        if (fseek(p->file, off, SEEK_SET) != 0) {
            memset(buf, 0xE5, MSX_SS);
        } else if (fread(buf, 1, MSX_SS, p->file) != MSX_SS) {
            memset(buf, 0xE5, MSX_SS);
        }
        uft_format_add_sector(track, (uint8_t)s, buf, MSX_SS,
                              (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

static uft_error_t msx_write_track(uft_disk_t *disk, int cyl, int head,
                                     const uft_track_t *track)
{
    msx_pd_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    long base = (long)((cyl * p->heads + head) * p->spt * MSX_SS);

    for (size_t s = 0; s < track->sector_count && (int)s < p->spt; s++) {
        long off = base + (long)s * MSX_SS;
        if (fseek(p->file, off, SEEK_SET) != 0) return UFT_ERROR_IO;

        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[MSX_SS];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, MSX_SS);
            data = pad;
        }
        if (fwrite(data, 1, MSX_SS, p->file) != MSX_SS) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_msx_disk = {
    .name = "MSX", .description = "MSX Disk Image (enhanced)",
    .extensions = "dsk;img", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = msx_probe, .open = msx_open, .close = msx_close,
    .read_track = msx_read_track, .write_track = msx_write_track,
};
UFT_REGISTER_FORMAT_PLUGIN(msx_disk)
