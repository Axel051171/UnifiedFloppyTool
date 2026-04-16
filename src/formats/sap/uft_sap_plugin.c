/**
 * @file uft_sap_plugin.c
 * @brief Thomson MO/TO SAP Format Plugin-B
 *
 * SAP (Systeme d'Archivage Pukall) is the standard disk image format
 * for Thomson MO5/MO6/TO7/TO8/TO9 computers.
 *
 * Header:
 *   Bytes 0-2: "SAP" magic
 *   Byte 3:    Version (0x00 = FM/SD, 0x01 = MFM/DD)
 *   Bytes 4-65 (0x42 total): zero padding
 *
 * Sector layout per track:
 *   Each sector has: 4-byte header + sector_data + 2-byte CRC
 *   Sector header: format(1) + protection(1) + track(1) + sector(1)
 *
 * Geometry:
 *   80 tracks, 1 side, 16 sectors
 *   v0 (FM):  128 bytes/sector, 16 sectors/track
 *   v1 (MFM): 256 bytes/sector, 16 sectors/track
 *
 * CRC: CRC-16-CCITT on sector data
 *
 * Reference: DCMOTO emulator, SAP format specification by Alexandre Pukall
 */

#include "uft/uft_format_common.h"

#define SAP_HEADER_SIZE   66  /* "SAP" + version + 62 bytes padding */
#define SAP_TRACKS        80
#define SAP_SIDES         1
#define SAP_SPT           16
#define SAP_SEC_HDR       4   /* format + protection + track + sector */
#define SAP_CRC_SIZE      2

#define SAP_VERSION_FM    0x00
#define SAP_VERSION_MFM   0x01

/* CRC-16-CCITT used by SAP format */
static uint16_t sap_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

typedef struct {
    uint8_t *data;        /* Entire file in memory */
    size_t   data_size;
    uint8_t  version;     /* 0=FM, 1=MFM */
    uint16_t sector_size; /* 128 or 256 */
} sap_pd_t;

static bool sap_plugin_probe(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence)
{
    (void)file_size;
    if (size < 4) return false;

    /* Check "SAP" magic at offset 0 */
    if (data[0] != 'S' || data[1] != 'A' || data[2] != 'P')
        return false;

    /* Version byte must be 0x00 (FM) or 0x01 (MFM) */
    if (data[3] != SAP_VERSION_FM && data[3] != SAP_VERSION_MFM)
        return false;

    *confidence = 95;
    return true;
}

static uft_error_t sap_plugin_open(uft_disk_t *disk, const char *path, bool ro)
{
    (void)ro;
    size_t file_size = 0;
    uint8_t *fdata = uft_read_file(path, &file_size);
    if (!fdata || file_size < SAP_HEADER_SIZE) {
        free(fdata);
        return UFT_ERROR_FORMAT_INVALID;
    }

    /* Verify magic */
    if (fdata[0] != 'S' || fdata[1] != 'A' || fdata[2] != 'P') {
        free(fdata);
        return UFT_ERROR_FORMAT_INVALID;
    }

    uint8_t version = fdata[3];
    if (version != SAP_VERSION_FM && version != SAP_VERSION_MFM) {
        free(fdata);
        return UFT_ERROR_FORMAT_INVALID;
    }

    uint16_t sec_size = (version == SAP_VERSION_FM) ? 128 : 256;

    /* Validate expected file size:
     * header + tracks * sectors * (sec_hdr + sec_data + crc) */
    size_t sector_record = SAP_SEC_HDR + sec_size + SAP_CRC_SIZE;
    size_t expected = SAP_HEADER_SIZE +
                      (size_t)SAP_TRACKS * SAP_SPT * sector_record;
    if (file_size < expected) {
        free(fdata);
        return UFT_ERROR_FORMAT_INVALID;
    }

    sap_pd_t *p = calloc(1, sizeof(sap_pd_t));
    if (!p) { free(fdata); return UFT_ERROR_NO_MEMORY; }
    p->data = fdata;
    p->data_size = file_size;
    p->version = version;
    p->sector_size = sec_size;

    disk->plugin_data = p;
    disk->geometry.cylinders = SAP_TRACKS;
    disk->geometry.heads = SAP_SIDES;
    disk->geometry.sectors = SAP_SPT;
    disk->geometry.sector_size = sec_size;
    disk->geometry.total_sectors = SAP_TRACKS * SAP_SIDES * SAP_SPT;
    return UFT_OK;
}

static void sap_plugin_close(uft_disk_t *disk)
{
    sap_pd_t *p = disk->plugin_data;
    if (p) {
        free(p->data);
        free(p);
        disk->plugin_data = NULL;
    }
}

static uft_error_t sap_plugin_read_track(uft_disk_t *disk, int cyl, int head,
                                          uft_track_t *track)
{
    sap_pd_t *p = disk->plugin_data;
    if (!p || !p->data) return UFT_ERROR_INVALID_STATE;
    if (head != 0) return UFT_ERROR_INVALID_STATE;
    if (cyl < 0 || cyl >= SAP_TRACKS) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    size_t sector_record = SAP_SEC_HDR + p->sector_size + SAP_CRC_SIZE;
    size_t track_offset = SAP_HEADER_SIZE +
                          (size_t)cyl * SAP_SPT * sector_record;

    uint8_t fill_buf[256];
    memset(fill_buf, 0xE5, sizeof(fill_buf));

    for (int s = 0; s < SAP_SPT; s++) {
        size_t sec_offset = track_offset + (size_t)s * sector_record;

        /* Bounds check */
        if (sec_offset + sector_record > p->data_size) {
            /* Missing sector: forensic fill */
            uft_format_add_sector(track, (uint8_t)s, fill_buf, p->sector_size,
                                  (uint8_t)cyl, 0);
            continue;
        }

        /* Read sector header: format(1) + protection(1) + track(1) + sector(1) */
        const uint8_t *sec_hdr = p->data + sec_offset;
        /* uint8_t fmt      = sec_hdr[0]; */
        /* uint8_t prot     = sec_hdr[1]; */
        /* uint8_t sec_track = sec_hdr[2]; */
        uint8_t sec_num  = sec_hdr[3];

        /* Sector data follows the 4-byte header */
        const uint8_t *sec_data = p->data + sec_offset + SAP_SEC_HDR;

        /* CRC follows sector data */
        const uint8_t *crc_bytes = sec_data + p->sector_size;
        uint16_t stored_crc = ((uint16_t)crc_bytes[0] << 8) | crc_bytes[1];
        uint16_t calc_crc = sap_crc16(sec_data, p->sector_size);

        /* Use sector number from header (1-based in SAP) */
        uint8_t sec_idx = (sec_num > 0) ? (sec_num - 1) : (uint8_t)s;

        uft_format_add_sector(track, sec_idx, sec_data, p->sector_size,
                              (uint8_t)cyl, 0);

        /* Set CRC status based on validation */
        if (track->sector_count > 0) {
            track->sectors[track->sector_count - 1].crc_ok =
                (stored_crc == calc_crc);
        }
    }
    return UFT_OK;
}

static uft_error_t sap_plugin_write_track(uft_disk_t *disk, int cyl, int head,
                                            const uft_track_t *track) {
    sap_pd_t *p = disk->plugin_data;
    if (!p || !p->data) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    if (head != 0) return UFT_ERROR_INVALID_STATE;
    if (cyl < 0 || cyl >= SAP_TRACKS) return UFT_ERROR_INVALID_STATE;

    size_t sector_record = SAP_SEC_HDR + p->sector_size + SAP_CRC_SIZE;
    size_t track_offset = SAP_HEADER_SIZE +
                          (size_t)cyl * SAP_SPT * sector_record;

    for (int s = 0; s < SAP_SPT; s++) {
        size_t sec_offset = track_offset + (size_t)s * sector_record;
        if (sec_offset + sector_record > p->data_size) break;

        /* Read sector number from SAP header to find matching input sector */
        uint8_t sec_num = p->data[sec_offset + 3];
        uint8_t sec_idx = (sec_num > 0) ? (sec_num - 1) : (uint8_t)s;

        /* Find matching sector in input track */
        for (size_t ts = 0; ts < track->sector_count; ts++) {
            if (track->sectors[ts].id.sector == sec_idx) {
                const uint8_t *src = track->sectors[ts].data;
                if (src && track->sectors[ts].data_len >= p->sector_size) {
                    /* Write sector data after the 4-byte header */
                    uint8_t *sec_data = p->data + sec_offset + SAP_SEC_HDR;
                    memcpy(sec_data, src, p->sector_size);

                    /* Update CRC-16-CCITT */
                    uint16_t crc = sap_crc16(sec_data, p->sector_size);
                    uint8_t *crc_bytes = sec_data + p->sector_size;
                    crc_bytes[0] = (crc >> 8) & 0xFF;
                    crc_bytes[1] = crc & 0xFF;
                }
                break;
            }
        }
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_sap_thomson = {
    .name = "SAP",
    .description = "Thomson MO/TO SAP",
    .extensions = "sap",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = sap_plugin_probe,
    .open = sap_plugin_open,
    .close = sap_plugin_close,
    .read_track = sap_plugin_read_track,
    .write_track = sap_plugin_write_track,
};
UFT_REGISTER_FORMAT_PLUGIN(sap_thomson)
