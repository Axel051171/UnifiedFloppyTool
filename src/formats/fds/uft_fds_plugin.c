/**
 * @file uft_fds_plugin.c
 * @brief Famicom Disk System (FDS) Plugin
 *
 * FDS images have an optional 16-byte fwNES header ("FDS\x1A")
 * followed by one or more 65500-byte disk sides.
 *
 * Each side contains:
 *   Block 1 (Disk Info):   1 type byte + 55 data = 56 bytes
 *   Block 2 (File Amount): 1 type byte + 1 data  = 2 bytes
 *   Block 3+4 (per file):  File header + data blocks
 *
 * The first 14 bytes of Block 1 data should be "*NINTENDO-HVC*".
 *
 * Since FDS is not a track/sector format (it uses a modified MFM
 * encoding), we map each 65500-byte side as one "track" with
 * 128 virtual 512-byte sectors for tool compatibility.
 *
 * Reference: https://wiki.nesdev.org/w/index.php/FDS_file_format
 */

#include "uft/uft_format_common.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FDS_FWNES_HEADER    16
#define FDS_SIDE_SIZE       65500
#define FDS_MAGIC           "FDS\x1A"
#define FDS_NINTENDO_SIG    "*NINTENDO-HVC*"
#define FDS_VIRTUAL_SPT     128     /* 65500 / 512 ~ 128 sectors */
#define FDS_VIRTUAL_SS      512
#define FDS_MAX_SIDES       8

/* ============================================================================
 * Plugin data
 * ============================================================================ */

typedef struct {
    FILE*       file;
    bool        has_header;     /* fwNES header present */
    uint8_t     side_count;
    size_t      data_offset;    /* offset to first side data */
} fds_data_t;

/* ============================================================================
 * probe
 * ============================================================================ */

bool fds_probe(const uint8_t *data, size_t size, size_t file_size,
               int *confidence)
{
    if (size < 56) return false;

    /* Check fwNES header */
    if (size >= FDS_FWNES_HEADER && memcmp(data, FDS_MAGIC, 4) == 0) {
        /* Side count must be sane */
        if (data[4] >= 1 && data[4] <= FDS_MAX_SIDES) {
            /* Verify expected size */
            size_t expected = FDS_FWNES_HEADER +
                              (size_t)data[4] * FDS_SIDE_SIZE;
            if (file_size >= expected) {
                *confidence = 95;
                return true;
            }
        }
        *confidence = 70;
        return true;
    }

    /* Raw (headerless): check for Nintendo signature at offset 1 */
    if (size >= 56 && memcmp(data + 1, FDS_NINTENDO_SIG, 14) == 0) {
        if (file_size >= FDS_SIDE_SIZE) {
            *confidence = 90;
            return true;
        }
    }

    /* Size-based: must be multiple of FDS_SIDE_SIZE */
    if (file_size >= FDS_SIDE_SIZE &&
        file_size % FDS_SIDE_SIZE == 0) {
        *confidence = 30;
        return true;
    }

    return false;
}

/* ============================================================================
 * open
 * ============================================================================ */

static uft_error_t fds_open(uft_disk_t *disk, const char *path,
                             bool read_only)
{
    (void)read_only;
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;

    /* Get file size */
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long fs = ftell(f);
    if (fs < 0) { fclose(f); return UFT_ERROR_IO; }
    size_t file_size = (size_t)fs;
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    /* Check for fwNES header */
    uint8_t hdr[FDS_FWNES_HEADER];
    if (fread(hdr, 1, FDS_FWNES_HEADER, f) != FDS_FWNES_HEADER) {
        fclose(f);
        return UFT_ERROR_IO;
    }

    fds_data_t *pdata = calloc(1, sizeof(fds_data_t));
    if (!pdata) { fclose(f); return UFT_ERROR_NO_MEMORY; }

    pdata->file = f;

    if (memcmp(hdr, FDS_MAGIC, 4) == 0) {
        pdata->has_header = true;
        pdata->side_count = hdr[4];
        pdata->data_offset = FDS_FWNES_HEADER;
    } else {
        pdata->has_header = false;
        pdata->data_offset = 0;
        pdata->side_count = (uint8_t)((file_size) / FDS_SIDE_SIZE);
    }

    if (pdata->side_count == 0 || pdata->side_count > FDS_MAX_SIDES) {
        free(pdata);
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }

    disk->plugin_data = pdata;
    disk->geometry.cylinders = pdata->side_count; /* 1 cyl per side */
    disk->geometry.heads = 1;
    disk->geometry.sectors = FDS_VIRTUAL_SPT;
    disk->geometry.sector_size = FDS_VIRTUAL_SS;
    disk->geometry.total_sectors =
        (uint32_t)pdata->side_count * FDS_VIRTUAL_SPT;

    return UFT_OK;
}

/* ============================================================================
 * close
 * ============================================================================ */

static void fds_close(uft_disk_t *disk)
{
    fds_data_t *pdata = disk->plugin_data;
    if (pdata) {
        if (pdata->file) fclose(pdata->file);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

/* ============================================================================
 * read_track — each "cylinder" is one disk side
 * ============================================================================ */

static uft_error_t fds_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track)
{
    fds_data_t *pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_INVALID_STATE;
    if (head != 0 || cyl >= pdata->side_count)
        return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    long offset = (long)(pdata->data_offset + (size_t)cyl * FDS_SIDE_SIZE);
    if (fseek(pdata->file, offset, SEEK_SET) != 0)
        return UFT_ERROR_IO;

    /* Read side data in 512-byte virtual sectors */
    uint8_t buf[FDS_VIRTUAL_SS];
    size_t remaining = FDS_SIDE_SIZE;

    for (int s = 0; s < FDS_VIRTUAL_SPT && remaining > 0; s++) {
        size_t chunk = (remaining < FDS_VIRTUAL_SS) ? remaining : FDS_VIRTUAL_SS;
        memset(buf, 0, FDS_VIRTUAL_SS);

        if (fread(buf, 1, chunk, pdata->file) != chunk)
            return UFT_ERROR_IO;

        uft_format_add_sector(track, (uint8_t)s, buf,
                              FDS_VIRTUAL_SS, (uint8_t)cyl, 0);
        remaining -= chunk;
    }

    return UFT_OK;
}

/* ============================================================================
 * Plugin registration
 * ============================================================================ */

const uft_format_plugin_t uft_format_plugin_fds = {
    .name         = "FDS",
    .description  = "Famicom Disk System",
    .extensions   = "fds",
    .version      = 0x00010000,
    .format       = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ,
    .probe        = fds_probe,
    .open         = fds_open,
    .close        = fds_close,
    .read_track   = fds_read_track,
};

UFT_REGISTER_FORMAT_PLUGIN(fds)
