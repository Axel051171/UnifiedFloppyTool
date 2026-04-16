/**
 * @file uft_d77.c
 * @brief D77 Disk Image Plugin (Sharp X1, FM-7, PC-88 compatible)
 *
 * D77 is the Sharp/Fujitsu variant of the NEC D88 disk image format.
 * Header layout is identical to D88:
 *
 *   Offset  Size  Description
 *   0x00    17    Disk name (null-terminated)
 *   0x1A    1     Write protect (0x00=off, 0x10=on)
 *   0x1B    1     Media type (0x00=2D, 0x10=2DD, 0x20=2HD)
 *   0x1C    4     Disk size in bytes (LE32)
 *   0x20    656   Track offset table (164 x LE32)
 *
 * Each track: sequence of sector headers (16 bytes) + data:
 *   Offset  Size  Description
 *   0x00    1     Cylinder
 *   0x01    1     Head
 *   0x02    1     Sector number (1-based)
 *   0x03    1     Size code (N: 128 << N)
 *   0x04    2     Number of sectors on track (LE16)
 *   0x06    1     Density (0x00=double, 0x40=single)
 *   0x07    1     Deleted mark (0x00=normal, 0x10=deleted)
 *   0x08    1     Status (0x00=OK, non-zero=error)
 *   0x09-0D 5     Reserved
 *   0x0E    2     Data size in bytes (LE16)
 *   0x10    N     Sector data
 *
 * Reference: D77/D88 format specification
 * Platforms: Sharp X1, FM-7/FM-77, PC-8801 (via D88 compat)
 */

#include "uft/uft_format_common.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define D77_HEADER_SIZE     0x2B0   /* 688 bytes */
#define D77_MAX_TRACKS      164     /* 82 cylinders x 2 heads */
#define D77_SEC_HDR_SIZE    16
#define D77_MAX_SECTOR_SIZE 8192
#define D77_MAX_SPT         64

/* Media type codes */
#define D77_MEDIA_2D        0x00
#define D77_MEDIA_2DD       0x10
#define D77_MEDIA_2HD       0x20

/* ============================================================================
 * Plugin data
 * ============================================================================ */

typedef struct {
    FILE*       file;
    uint8_t     media_type;
    uint32_t    disk_size;
    uint32_t    track_offsets[D77_MAX_TRACKS];
    char        disk_name[17];
} d77_data_t;

/* ============================================================================
 * probe
 * ============================================================================ */

bool d77_probe(const uint8_t *data, size_t size, size_t file_size,
               int *confidence)
{
    if (size < D77_HEADER_SIZE) return false;

    uint32_t disk_size = uft_read_le32(data + 0x1C);
    uint8_t media = data[0x1B];

    /* Disk size must be plausible */
    if (disk_size == 0 || disk_size > file_size) return false;

    /* Media type must be known */
    if (media != D77_MEDIA_2D && media != D77_MEDIA_2DD &&
        media != D77_MEDIA_2HD)
        return false;

    /* First track offset should point past header */
    uint32_t first_off = uft_read_le32(data + 0x20);
    if (first_off < D77_HEADER_SIZE || first_off >= disk_size)
        return false;

    *confidence = 85;
    return true;
}

/* ============================================================================
 * open
 * ============================================================================ */

static uft_error_t d77_open(uft_disk_t *disk, const char *path,
                             bool read_only)
{
    FILE *f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;

    uint8_t hdr[D77_HEADER_SIZE];
    if (fread(hdr, 1, D77_HEADER_SIZE, f) != D77_HEADER_SIZE) {
        fclose(f);
        return UFT_ERROR_IO;
    }

    d77_data_t *pdata = calloc(1, sizeof(d77_data_t));
    if (!pdata) { fclose(f); return UFT_ERROR_NO_MEMORY; }

    pdata->file = f;

    /* Parse header */
    memcpy(pdata->disk_name, hdr, 16);
    pdata->disk_name[16] = '\0';
    pdata->media_type = hdr[0x1B];
    pdata->disk_size = uft_read_le32(hdr + 0x1C);

    /* Read track offset table */
    uint8_t track_count = 0;
    for (int i = 0; i < D77_MAX_TRACKS; i++) {
        pdata->track_offsets[i] = uft_read_le32(hdr + 0x20 + i * 4);
        if (pdata->track_offsets[i] != 0)
            track_count = (uint8_t)(i + 1);
    }

    /* Determine geometry from media type and track count */
    uint8_t cylinders = (track_count + 1) / 2;
    if (cylinders == 0) cylinders = 40;
    if (cylinders > 82) cylinders = 82;
    uint8_t heads = (track_count > cylinders) ? 2 : 1;

    /* Read actual sector parameters from first track header */
    uint16_t sector_size = 256;
    uint16_t sectors_per_track = 16;

    if (pdata->track_offsets[0] >= D77_HEADER_SIZE) {
        uint8_t sec_hdr[D77_SEC_HDR_SIZE];
        if (fseek(f, (long)pdata->track_offsets[0], SEEK_SET) == 0 &&
            fread(sec_hdr, 1, D77_SEC_HDR_SIZE, f) == D77_SEC_HDR_SIZE) {
            uint16_t ss = uft_read_le16(sec_hdr + 14);
            uint16_t sc = uft_read_le16(sec_hdr + 4);
            if (ss > 0 && ss <= D77_MAX_SECTOR_SIZE &&
                sc > 0 && sc <= D77_MAX_SPT) {
                sector_size = ss;
                sectors_per_track = sc;
            }
        }
    }

    disk->plugin_data = pdata;
    disk->geometry.cylinders = cylinders;
    disk->geometry.heads = heads;
    disk->geometry.sectors = sectors_per_track;
    disk->geometry.sector_size = sector_size;
    disk->geometry.total_sectors =
        (uint32_t)cylinders * heads * sectors_per_track;

    return UFT_OK;
}

/* ============================================================================
 * close
 * ============================================================================ */

static void d77_close(uft_disk_t *disk)
{
    d77_data_t *pdata = disk->plugin_data;
    if (pdata) {
        if (pdata->file) fclose(pdata->file);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

/* ============================================================================
 * read_track
 * ============================================================================ */

static uft_error_t d77_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track)
{
    d77_data_t *pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_INVALID_STATE;

    int idx = cyl * disk->geometry.heads + head;
    if (idx >= D77_MAX_TRACKS) return UFT_ERROR_INVALID_STATE;
    if (pdata->track_offsets[idx] == 0) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    if (fseek(pdata->file, (long)pdata->track_offsets[idx], SEEK_SET) != 0)
        return UFT_ERROR_IO;

    /* Read sector headers + data until end of track */
    for (int s = 0; s < D77_MAX_SPT; s++) {
        uint8_t sec_hdr[D77_SEC_HDR_SIZE];
        if (fread(sec_hdr, 1, D77_SEC_HDR_SIZE, pdata->file) !=
            D77_SEC_HDR_SIZE)
            break;

        uint16_t data_size = uft_read_le16(sec_hdr + 14);
        uint16_t num_sectors = uft_read_le16(sec_hdr + 4);

        /* Sanity check */
        if (data_size == 0 || data_size > D77_MAX_SECTOR_SIZE) break;
        if (num_sectors == 0) break;

        /* Only read num_sectors sectors total */
        if (s >= num_sectors) break;

        uint8_t *buf = malloc(data_size);
        if (!buf) return UFT_ERROR_NO_MEMORY;

        if (fread(buf, 1, data_size, pdata->file) != data_size) {
            free(buf);
            return UFT_ERROR_IO;
        }

        /* Sector number is 1-based in header, convert to 0-based for add */
        uint8_t sec_num = sec_hdr[2];
        if (sec_num > 0) sec_num--;

        uft_format_add_sector(track, sec_num, buf, data_size,
                              (uint8_t)cyl, (uint8_t)head);
        /* D77 sector flags: byte 7=deleted (0x10), byte 8=status (0x00=OK) */
        if (track->sector_count > 0) {
            if (sec_hdr[7] == 0x10)
                track->sectors[track->sector_count - 1].deleted = true;
            if (sec_hdr[8] != 0x00)
                track->sectors[track->sector_count - 1].crc_ok = false;
        }
        free(buf);
    }

    return UFT_OK;
}

/* ============================================================================
 * write_track
 * ============================================================================ */

static uft_error_t d77_write_track(uft_disk_t *disk, int cyl, int head,
                                    const uft_track_t *track)
{
    d77_data_t *pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    int idx = cyl * disk->geometry.heads + head;
    if (idx >= D77_MAX_TRACKS) return UFT_ERROR_INVALID_STATE;
    if (pdata->track_offsets[idx] == 0) return UFT_ERROR_INVALID_STATE;

    if (fseek(pdata->file, (long)pdata->track_offsets[idx], SEEK_SET) != 0)
        return UFT_ERROR_IO;

    /* Walk sector headers, write only the data portion */
    for (int s = 0; s < D77_MAX_SPT; s++) {
        uint8_t sec_hdr[D77_SEC_HDR_SIZE];
        if (fread(sec_hdr, 1, D77_SEC_HDR_SIZE, pdata->file) !=
            D77_SEC_HDR_SIZE)
            break;

        uint16_t data_size = uft_read_le16(sec_hdr + 14);
        uint16_t num_sectors = uft_read_le16(sec_hdr + 4);

        if (data_size == 0 || data_size > D77_MAX_SECTOR_SIZE) break;
        if (num_sectors == 0) break;
        if (s >= num_sectors) break;

        /* Write sector data if we have a matching sector in track */
        if ((size_t)s < track->sector_count) {
            const uint8_t *data = track->sectors[s].data;
            uint8_t *pad = NULL;
            if (!data || track->sectors[s].data_len == 0) {
                pad = malloc(data_size);
                if (!pad) return UFT_ERROR_NO_MEMORY;
                memset(pad, 0xE5, data_size);
                data = pad;
            }
            if (fwrite(data, 1, data_size, pdata->file) != data_size) {
                free(pad);
                return UFT_ERROR_IO;
            }
            free(pad);
        } else {
            /* Skip past this sector's data */
            if (fseek(pdata->file, (long)data_size, SEEK_CUR) != 0)
                return UFT_ERROR_IO;
        }
    }

    return UFT_OK;
}

/* ============================================================================
 * Plugin registration
 * ============================================================================ */

const uft_format_plugin_t uft_format_plugin_d77 = {
    .name         = "D77",
    .description  = "Sharp X1/FM-7 Disk Image (D77/1DD)",
    .extensions   = "d77;1dd",
    .version      = 0x00010000,
    .format       = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe        = d77_probe,
    .open         = d77_open,
    .close        = d77_close,
    .read_track   = d77_read_track,
    .write_track  = d77_write_track,
};

UFT_REGISTER_FORMAT_PLUGIN(d77)
