/**
 * @file uft_dim.c
 * @brief DIM Disk Image Plugin (Sharp X68000)
 *
 * DIM is the standard disk image format for the Sharp X68000 series.
 * It consists of a 256-byte header followed by raw sector data.
 *
 * Header layout:
 *   Offset  Size  Description
 *   0x00    1     Media type (0x00=2HD, 0x01=2HS, 0x02=2HC, 0x09=2HQ, ...)
 *   0x01-AA       Reserved/track flags
 *   0xAB    1     Overtrack flag
 *   0xAC-FF       Reserved
 *
 * Media types and geometry:
 *   0x00 2HD:  77 cyl, 2 heads,  8 spt, 1024 byte/sec = 1,261,568
 *   0x01 2HS:  77 cyl, 2 heads,  8 spt, 1024 byte/sec
 *   0x02 2HC:  77 cyl, 2 heads,  8 spt, 1024 byte/sec
 *   0x03 2HDE: 77 cyl, 2 heads,  8 spt, 1024 byte/sec
 *   0x09 2HQ:  80 cyl, 2 heads, 18 spt,  512 byte/sec = 1,474,560
 *   0x11 2DD8: 80 cyl, 2 heads,  8 spt,  512 byte/sec
 *   0x19 2DD9: 80 cyl, 2 heads,  9 spt,  512 byte/sec = 737,280
 *
 * Reference: X68000 DIM format specification
 */

#include "uft/uft_format_common.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define DIM_HEADER_SIZE     256
#define DIM_MAX_SECTOR_SIZE 1024
#define DIM_MAX_CYLINDERS   82
#define DIM_MAX_SPT         18

/* Media type codes */
#define DIM_MEDIA_2HD       0x00
#define DIM_MEDIA_2HS       0x01
#define DIM_MEDIA_2HC       0x02
#define DIM_MEDIA_2HDE      0x03
#define DIM_MEDIA_2HQ       0x09
#define DIM_MEDIA_2DD_8     0x11
#define DIM_MEDIA_2DD_9     0x19

/* ============================================================================
 * Geometry lookup
 * ============================================================================ */

static bool dim_get_geometry(uint8_t media,
                             uint8_t *cyl, uint8_t *heads,
                             uint8_t *spt, uint16_t *sector_size)
{
    switch (media) {
        case DIM_MEDIA_2HD:
        case DIM_MEDIA_2HS:
        case DIM_MEDIA_2HC:
        case DIM_MEDIA_2HDE:
            *cyl = 77; *heads = 2; *spt = 8; *sector_size = 1024;
            return true;
        case DIM_MEDIA_2HQ:
            *cyl = 80; *heads = 2; *spt = 18; *sector_size = 512;
            return true;
        case DIM_MEDIA_2DD_8:
            *cyl = 80; *heads = 2; *spt = 8; *sector_size = 512;
            return true;
        case DIM_MEDIA_2DD_9:
            *cyl = 80; *heads = 2; *spt = 9; *sector_size = 512;
            return true;
        default:
            return false;
    }
}

/* ============================================================================
 * Plugin data
 * ============================================================================ */

typedef struct {
    FILE*       file;
    uint8_t     media_type;
    uint8_t     cylinders;
    uint8_t     heads;
    uint8_t     sectors_per_track;
    uint16_t    sector_size;
} dim_data_t;

/* ============================================================================
 * probe
 * ============================================================================ */

bool dim_probe(const uint8_t *data, size_t size, size_t file_size,
               int *confidence)
{
    (void)data;
    if (size < DIM_HEADER_SIZE) return false;

    uint8_t media = data[0];
    uint8_t cyl, heads, spt;
    uint16_t ss;

    if (!dim_get_geometry(media, &cyl, &heads, &spt, &ss))
        return false;

    /* Validate file size: header + expected data */
    uint32_t expected = DIM_HEADER_SIZE +
                        (uint32_t)cyl * heads * spt * ss;
    if (file_size < expected)
        return false;

    *confidence = 85;
    return true;
}

/* ============================================================================
 * open
 * ============================================================================ */

static uft_error_t dim_open(uft_disk_t *disk, const char *path,
                             bool read_only)
{
    FILE *f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;

    uint8_t hdr[DIM_HEADER_SIZE];
    if (fread(hdr, 1, DIM_HEADER_SIZE, f) != DIM_HEADER_SIZE) {
        fclose(f);
        return UFT_ERROR_IO;
    }

    uint8_t cyl, heads, spt;
    uint16_t ss;
    if (!dim_get_geometry(hdr[0], &cyl, &heads, &spt, &ss)) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }

    dim_data_t *pdata = calloc(1, sizeof(dim_data_t));
    if (!pdata) { fclose(f); return UFT_ERROR_NO_MEMORY; }

    pdata->file = f;
    pdata->media_type = hdr[0];
    pdata->cylinders = cyl;
    pdata->heads = heads;
    pdata->sectors_per_track = spt;
    pdata->sector_size = ss;

    disk->plugin_data = pdata;
    disk->geometry.cylinders = cyl;
    disk->geometry.heads = heads;
    disk->geometry.sectors = spt;
    disk->geometry.sector_size = ss;
    disk->geometry.total_sectors = (uint32_t)cyl * heads * spt;

    return UFT_OK;
}

/* ============================================================================
 * close
 * ============================================================================ */

static void dim_close(uft_disk_t *disk)
{
    dim_data_t *pdata = disk->plugin_data;
    if (pdata) {
        if (pdata->file) fclose(pdata->file);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

/* ============================================================================
 * read_track
 * ============================================================================ */

static uft_error_t dim_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track)
{
    dim_data_t *pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_INVALID_STATE;
    if (cyl >= pdata->cylinders || head >= pdata->heads)
        return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    /* Data starts after 256-byte header, tracks are sequential */
    uint32_t track_index = (uint32_t)cyl * pdata->heads + (uint32_t)head;
    long offset = DIM_HEADER_SIZE +
                  (long)(track_index * pdata->sectors_per_track *
                         pdata->sector_size);

    if (fseek(pdata->file, offset, SEEK_SET) != 0)
        return UFT_ERROR_IO;

    uint8_t buf[DIM_MAX_SECTOR_SIZE];

    for (int s = 0; s < pdata->sectors_per_track; s++) {
        if (fread(buf, 1, pdata->sector_size, pdata->file) !=
            pdata->sector_size)
            return UFT_ERROR_IO;

        uft_format_add_sector(track, (uint8_t)s, buf,
                              pdata->sector_size,
                              (uint8_t)cyl, (uint8_t)head);
    }

    return UFT_OK;
}

/* ============================================================================
 * write_track
 * ============================================================================ */

static uft_error_t dim_write_track(uft_disk_t *disk, int cyl, int head,
                                    const uft_track_t *track)
{
    dim_data_t *pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    if (cyl >= pdata->cylinders || head >= pdata->heads)
        return UFT_ERROR_INVALID_STATE;

    uint32_t track_index = (uint32_t)cyl * pdata->heads + (uint32_t)head;
    long offset = DIM_HEADER_SIZE +
                  (long)(track_index * pdata->sectors_per_track *
                         pdata->sector_size);

    for (size_t s = 0; s < track->sector_count &&
         (int)s < pdata->sectors_per_track; s++) {
        if (fseek(pdata->file, offset + (long)s * pdata->sector_size,
                  SEEK_SET) != 0)
            return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[DIM_MAX_SECTOR_SIZE];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, pdata->sector_size);
            data = pad;
        }
        if (fwrite(data, 1, pdata->sector_size, pdata->file) !=
            pdata->sector_size)
            return UFT_ERROR_IO;
    }
    return UFT_OK;
}

/* ============================================================================
 * Plugin registration
 * ============================================================================ */

const uft_format_plugin_t uft_format_plugin_dim = {
    .name         = "DIM",
    .description  = "Sharp X68000 Disk Image",
    .extensions   = "dim;xdf",
    .version      = 0x00010000,
    .format       = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe        = dim_probe,
    .open         = dim_open,
    .close        = dim_close,
    .read_track   = dim_read_track,
    .write_track  = dim_write_track,
    .verify_track = uft_generic_verify_track,
};

UFT_REGISTER_FORMAT_PLUGIN(dim)
