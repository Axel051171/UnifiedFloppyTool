/**
 * @file uft_udi_plugin.c
 * @brief UDI (Ultra Disk Image) Plugin-B — Spectrum MFM/FM bitstream format
 *
 * UDI preserves raw MFM/FM bitstream data per track.
 *
 * Header (16 bytes):
 *   0-3: "UDI!" magic
 *   4:   file format version
 *   5:   max cylinder number (N cylinders = value + 1)
 *   6:   max head number (N heads = value + 1)
 *   7:   unused
 *   8-11: extra info data size (LE32)
 *   12-15: reserved (zero)
 *
 * After header: for each cylinder 0..max_cyl, head 0..max_head:
 *   Track type (1 byte): 0x00 = MFM, 0x01 = FM
 *   Track data length (2 bytes LE): length in bytes of bitstream
 *   Track data: raw MFM/FM bitstream (length bytes)
 *   Optional: (length+7)/8 bytes of clock mark data for MFM type 0x00
 *
 * After all tracks: CRC32 of entire file (4 bytes LE)
 *
 * Reference: UDI specification by Simon Owen, libdsk
 */
#include "uft/uft_format_common.h"

#define UDI_MAGIC       "UDI!"
#define UDI_HDR_SIZE    16

typedef struct {
    uint8_t *data;
    size_t   size;
    uint8_t  max_cyl;
    uint8_t  max_head;
} udi_pd_t;

static bool udi_probe(const uint8_t *data, size_t size, size_t file_size,
                       int *confidence)
{
    (void)file_size;
    if (size < UDI_HDR_SIZE) return false;
    if (memcmp(data, UDI_MAGIC, 4) == 0) {
        *confidence = 95;
        return true;
    }
    return false;
}

static uft_error_t udi_open(uft_disk_t *disk, const char *path, bool ro)
{
    (void)ro;
    size_t file_size = 0;
    uint8_t *data = uft_read_file(path, &file_size);
    if (!data || file_size < UDI_HDR_SIZE) {
        free(data);
        return UFT_ERROR_FILE_OPEN;
    }

    if (memcmp(data, UDI_MAGIC, 4) != 0) {
        free(data);
        return UFT_ERROR_FORMAT_INVALID;
    }

    uint8_t max_cyl = data[5];
    uint8_t max_head = data[6];

    if (max_cyl > 200 || max_head > 1) {
        free(data);
        return UFT_ERROR_FORMAT_INVALID;
    }

    udi_pd_t *p = calloc(1, sizeof(udi_pd_t));
    if (!p) { free(data); return UFT_ERROR_NO_MEMORY; }
    p->data = data;
    p->size = file_size;
    p->max_cyl = max_cyl;
    p->max_head = max_head;

    disk->plugin_data = p;
    disk->geometry.cylinders = max_cyl + 1;
    disk->geometry.heads = max_head + 1;
    disk->geometry.sectors = 0;     /* bitstream, no fixed SPT */
    disk->geometry.sector_size = 0; /* bitstream */
    disk->geometry.total_sectors = 0;
    return UFT_OK;
}

static void udi_close(uft_disk_t *disk)
{
    udi_pd_t *p = disk->plugin_data;
    if (p) {
        free(p->data);
        free(p);
        disk->plugin_data = NULL;
    }
}

/* Find the byte offset of track (cyl, head) in the file data.
 * Returns 0 if not found. Sets out_type and out_len if found. */
static size_t udi_find_track(const udi_pd_t *p, int cyl, int head,
                              uint8_t *out_type, uint16_t *out_len)
{
    size_t pos = UDI_HDR_SIZE;
    int num_heads = p->max_head + 1;

    for (int c = 0; c <= p->max_cyl; c++) {
        for (int h = 0; h < num_heads; h++) {
            if (pos + 3 > p->size) return 0;

            uint8_t ttype = p->data[pos];
            uint16_t tlen = (uint16_t)p->data[pos + 1] |
                            ((uint16_t)p->data[pos + 2] << 8);

            if (c == cyl && h == head) {
                *out_type = ttype;
                *out_len = tlen;
                return pos;
            }

            pos += 3 + tlen;
            /* MFM tracks have additional clock mark data */
            if (ttype == 0x00 && tlen > 0)
                pos += (tlen + 7) / 8;
        }
    }
    return 0;
}

static uft_error_t udi_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track)
{
    udi_pd_t *p = disk->plugin_data;
    if (!p || !p->data) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    uint8_t ttype = 0;
    uint16_t tlen = 0;
    size_t pos = udi_find_track(p, cyl, head, &ttype, &tlen);
    if (pos == 0) return UFT_OK; /* Track not present */

    /* Set encoding based on track type */
    track->encoding = (ttype == 0x01) ? UFT_ENC_FM : UFT_ENC_MFM;

    /* Store raw bitstream data */
    size_t data_off = pos + 3;
    if (data_off + tlen > p->size) return UFT_OK;

    /* Allocate and copy raw data into the track */
    track->raw_data = malloc(tlen);
    if (!track->raw_data) return UFT_ERROR_NO_MEMORY;
    memcpy(track->raw_data, p->data + data_off, tlen);
    track->raw_size = tlen;
    track->raw_len = tlen;
    track->raw_bits = (size_t)tlen * 8;
    track->raw_capacity = tlen;
    track->owns_data = true;

    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_udi = {
    .name = "UDI", .description = "Ultra Disk Image (Spectrum)",
    .extensions = "udi", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_FLUX,
    .probe = udi_probe, .open = udi_open, .close = udi_close,
    .read_track = udi_read_track,
};
UFT_REGISTER_FORMAT_PLUGIN(udi)
