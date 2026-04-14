/**
 * @file uft_dc42.c
 * @brief Apple DiskCopy 4.2 Plugin
 *
 * DiskCopy 4.2 was Apple's standard disk image format for Mac OS.
 * Used for 400K, 800K, and 1.44MB Macintosh floppy images.
 *
 * Header layout (84 bytes):
 *   Offset  Size  Description
 *   0x00    1     Name length (Pascal string)
 *   0x01    63    Disk name
 *   0x40    4     Data size (BE32)
 *   0x44    4     Tag size (BE32) — 0 for most images
 *   0x48    4     Data checksum (BE32)
 *   0x4C    4     Tag checksum (BE32)
 *   0x50    1     Disk format (0=400K GCR, 1=800K GCR, 2=720K MFM, 3=1440K MFM)
 *   0x51    1     Format byte (0x12=Mac 400K, 0x22=Mac 800K, 0x24=ProDOS 800K)
 *   0x52    2     Magic (0x0100)
 *
 * Data follows immediately at offset 84.
 *
 * Reference: Apple DiskCopy 4.2 format (Inside Macintosh)
 */

#include "uft/uft_format_common.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define DC42_HEADER_SIZE    84
#define DC42_MAGIC          0x0100
#define DC42_SECTOR_SIZE    512
#define DC42_MAX_NAME       63

/* Disk format codes */
#define DC42_FMT_400K_GCR   0x00    /* Mac 400K single-sided GCR */
#define DC42_FMT_800K_GCR   0x01    /* Mac 800K double-sided GCR */
#define DC42_FMT_720K_MFM   0x02    /* 720K MFM (PC compat) */
#define DC42_FMT_1440K_MFM  0x03    /* 1.44M MFM (SuperDrive) */

/* ============================================================================
 * Plugin data
 * ============================================================================ */

typedef struct {
    FILE*       file;
    uint32_t    data_size;
    uint32_t    tag_size;
    uint8_t     disk_format;
    uint8_t     cylinders;
    uint8_t     heads;
    uint8_t     sectors_per_track;
} dc42_data_t;

/* ============================================================================
 * Geometry from format code
 * ============================================================================ */

static bool dc42_get_geometry(uint8_t fmt, uint32_t data_size,
                              uint8_t *cyl, uint8_t *heads, uint8_t *spt)
{
    switch (fmt) {
        case DC42_FMT_400K_GCR:
            *cyl = 80; *heads = 1; *spt = 10;  /* variable SPT, use avg */
            return true;
        case DC42_FMT_800K_GCR:
            *cyl = 80; *heads = 2; *spt = 10;
            return true;
        case DC42_FMT_720K_MFM:
            *cyl = 80; *heads = 2; *spt = 9;
            return true;
        case DC42_FMT_1440K_MFM:
            *cyl = 80; *heads = 2; *spt = 18;
            return true;
        default:
            break;
    }

    /* Fallback: derive from data size */
    if (data_size == 0 || data_size % DC42_SECTOR_SIZE != 0) return false;
    uint32_t total = data_size / DC42_SECTOR_SIZE;

    if (total == 800)  { *cyl = 80; *heads = 1; *spt = 10; return true; }
    if (total == 1600) { *cyl = 80; *heads = 2; *spt = 10; return true; }
    if (total == 1440) { *cyl = 80; *heads = 2; *spt = 9;  return true; }
    if (total == 2880) { *cyl = 80; *heads = 2; *spt = 18; return true; }

    return false;
}

/* ============================================================================
 * probe
 * ============================================================================ */

bool dc42_probe(const uint8_t *data, size_t size, size_t file_size,
                int *confidence)
{
    (void)file_size;
    if (size < DC42_HEADER_SIZE) return false;

    /* Check magic at offset 82 (BE16) */
    uint16_t magic = ((uint16_t)data[82] << 8) | data[83];
    if (magic != DC42_MAGIC) return false;

    /* Name length must be sane */
    if (data[0] > DC42_MAX_NAME) return false;

    /* Data size must be non-zero */
    uint32_t data_size = uft_read_be32(data + 0x40);
    if (data_size == 0) return false;

    /* Disk format must be known */
    uint8_t fmt = data[0x50];
    if (fmt > DC42_FMT_1440K_MFM) {
        *confidence = 50;
    } else {
        *confidence = 92;
    }
    return true;
}

/* ============================================================================
 * open
 * ============================================================================ */

static uft_error_t dc42_open(uft_disk_t *disk, const char *path,
                              bool read_only)
{
    FILE *f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;

    uint8_t hdr[DC42_HEADER_SIZE];
    if (fread(hdr, 1, DC42_HEADER_SIZE, f) != DC42_HEADER_SIZE) {
        fclose(f);
        return UFT_ERROR_IO;
    }

    uint16_t magic = ((uint16_t)hdr[82] << 8) | hdr[83];
    if (magic != DC42_MAGIC) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }

    uint32_t data_size = uft_read_be32(hdr + 0x40);
    uint32_t tag_size = uft_read_be32(hdr + 0x44);
    uint8_t fmt = hdr[0x50];

    uint8_t cyl, heads, spt;
    if (!dc42_get_geometry(fmt, data_size, &cyl, &heads, &spt)) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }

    dc42_data_t *pdata = calloc(1, sizeof(dc42_data_t));
    if (!pdata) { fclose(f); return UFT_ERROR_NO_MEMORY; }

    pdata->file = f;
    pdata->data_size = data_size;
    pdata->tag_size = tag_size;
    pdata->disk_format = fmt;
    pdata->cylinders = cyl;
    pdata->heads = heads;
    pdata->sectors_per_track = spt;

    disk->plugin_data = pdata;
    disk->geometry.cylinders = cyl;
    disk->geometry.heads = heads;
    disk->geometry.sectors = spt;
    disk->geometry.sector_size = DC42_SECTOR_SIZE;
    disk->geometry.total_sectors = (uint32_t)cyl * heads * spt;

    return UFT_OK;
}

/* ============================================================================
 * close
 * ============================================================================ */

static void dc42_close(uft_disk_t *disk)
{
    dc42_data_t *pdata = disk->plugin_data;
    if (pdata) {
        if (pdata->file) fclose(pdata->file);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

/* ============================================================================
 * read_track
 * ============================================================================ */

static uft_error_t dc42_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track)
{
    dc42_data_t *pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_INVALID_STATE;
    if (cyl >= pdata->cylinders || head >= pdata->heads)
        return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    /* Sector data starts at offset 84, tracks are sequential */
    uint32_t track_idx = (uint32_t)cyl * pdata->heads + (uint32_t)head;
    long offset = DC42_HEADER_SIZE +
                  (long)(track_idx * pdata->sectors_per_track *
                         DC42_SECTOR_SIZE);

    /* Check bounds against data_size */
    uint32_t track_end = (uint32_t)(offset - DC42_HEADER_SIZE) +
                         (uint32_t)pdata->sectors_per_track * DC42_SECTOR_SIZE;
    if (track_end > pdata->data_size)
        return UFT_ERROR_INVALID_STATE;

    if (fseek(pdata->file, offset, SEEK_SET) != 0)
        return UFT_ERROR_IO;

    uint8_t buf[DC42_SECTOR_SIZE];

    for (int s = 0; s < pdata->sectors_per_track; s++) {
        if (fread(buf, 1, DC42_SECTOR_SIZE, pdata->file) != DC42_SECTOR_SIZE)
            return UFT_ERROR_IO;

        uft_format_add_sector(track, (uint8_t)s, buf,
                              DC42_SECTOR_SIZE,
                              (uint8_t)cyl, (uint8_t)head);
    }

    return UFT_OK;
}

/* ============================================================================
 * Plugin registration
 * ============================================================================ */

const uft_format_plugin_t uft_format_plugin_dc42 = {
    .name         = "DC42",
    .description  = "Apple DiskCopy 4.2",
    .extensions   = "dc42;image;img",
    .version      = 0x00010000,
    .format       = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ,
    .probe        = dc42_probe,
    .open         = dc42_open,
    .close        = dc42_close,
    .read_track   = dc42_read_track,
};

UFT_REGISTER_FORMAT_PLUGIN(dc42)
