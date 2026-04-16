/**
 * @file uft_fdi.c
 * @brief Formatted Disk Image (FDI) Plugin
 *
 * FDI header (14+ bytes):
 *   0-2:  "FDI" signature
 *   3:    Write protect flag
 *   4-5:  Cylinders (LE16)
 *   6-7:  Heads (LE16)
 *   8-9:  Description offset (LE16)
 *   10-11: Data offset (LE16)
 *   12-13: Additional info length (LE16)
 *
 * After header: track table (7 bytes per track):
 *   0-3:  Track data offset from data_offset (LE32)
 *   2:    Reserved
 *   3-4:  Sector count for this track (LE16)  -- at offset +4 in entry
 *   Each sector: 7-byte descriptor (C, H, R, N, flags, LE16 data_size)
 */
#include "uft/uft_format_common.h"

#define FDI_HEADER_SIZE 14

typedef struct {
    FILE*       file;
    uint16_t    cyls;
    uint16_t    heads;
    uint16_t    data_offset;
} fdi_data_t;

bool fdi_probe(const uint8_t *data, size_t size, size_t file_size,
               int *confidence) {
    (void)file_size;
    if (size >= 3 && memcmp(data, "FDI", 3) == 0) {
        *confidence = 95;
        return true;
    }
    return false;
}

static uft_error_t fdi_open(uft_disk_t *disk, const char *path, bool ro) {
    (void)ro;
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;

    uint8_t hdr[FDI_HEADER_SIZE];
    if (fread(hdr, 1, FDI_HEADER_SIZE, f) != FDI_HEADER_SIZE) {
        fclose(f); return UFT_ERROR_IO;
    }
    if (memcmp(hdr, "FDI", 3) != 0) {
        fclose(f); return UFT_ERROR_FORMAT_INVALID;
    }

    fdi_data_t *p = calloc(1, sizeof(fdi_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }

    p->file = f;
    p->cyls = uft_read_le16(hdr + 4);
    p->heads = uft_read_le16(hdr + 6);
    p->data_offset = uft_read_le16(hdr + 10);

    if (p->cyls == 0 || p->cyls > 84 || p->heads == 0 || p->heads > 2) {
        free(p); fclose(f); return UFT_ERROR_FORMAT_INVALID;
    }

    disk->plugin_data = p;
    disk->geometry.cylinders = p->cyls;
    disk->geometry.heads = p->heads;
    disk->geometry.sectors = 18;
    disk->geometry.sector_size = 512;
    disk->geometry.total_sectors = (uint32_t)p->cyls * p->heads * 18;
    return UFT_OK;
}

static void fdi_close(uft_disk_t *disk) {
    fdi_data_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t fdi_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track) {
    fdi_data_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    /* Track table starts at offset 14 + additional_info_length.
     * Each track entry is 7 bytes: offset(4) + reserved(1) + sectors(2) */
    int trk_idx = cyl * p->heads + head;

    /* Seek to track table entry */
    long entry_pos = FDI_HEADER_SIZE + (long)trk_idx * 7;
    if (fseek(p->file, entry_pos, SEEK_SET) != 0)
        return UFT_ERROR_IO;

    uint8_t entry[7];
    if (fread(entry, 1, 7, p->file) != 7)
        return UFT_ERROR_IO;

    uint32_t trk_offset = uft_read_le32(entry);
    uint16_t sec_count = uft_read_le16(entry + 5);

    if (sec_count == 0 || sec_count > 64) return UFT_OK;

    /* Read sector descriptors */
    long desc_pos = entry_pos + 7;
    if (fseek(p->file, desc_pos, SEEK_SET) != 0)
        return UFT_ERROR_IO;

    /* Each sector descriptor: C(1) H(1) R(1) N(1) flags(1) size(2) = 7 bytes */
    long data_pos = (long)p->data_offset + trk_offset;

    for (int s = 0; s < sec_count; s++) {
        uint8_t sec_desc[7];
        if (fread(sec_desc, 1, 7, p->file) != 7) break;

        uint8_t sec_n = sec_desc[3];
        uint16_t data_size = uft_read_le16(sec_desc + 5);
        uint16_t real_size = (sec_n < 7) ? (128 << sec_n) : data_size;

        if (data_size > 0 && real_size > 0) {
            long saved = ftell(p->file);
            if (saved < 0) break;

            if (fseek(p->file, data_pos, SEEK_SET) != 0) break;

            uint8_t *buf = malloc(real_size);
            if (!buf) break;

            size_t rd = fread(buf, 1, real_size, p->file);
            if (rd > 0) {
                uft_format_add_sector(track,
                    sec_desc[2] > 0 ? sec_desc[2] - 1 : 0,
                    buf, (uint16_t)rd,
                    (uint8_t)cyl, (uint8_t)head);
            }
            free(buf);
            data_pos += data_size;

            if (fseek(p->file, saved, SEEK_SET) != 0) break;
        } else {
            data_pos += data_size;
        }
    }

    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_fdi = {
    .name = "FDI", .description = "Formatted Disk Image",
    .extensions = "fdi", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ,
    .probe = fdi_probe, .open = fdi_open, .close = fdi_close,
    .read_track = fdi_read_track,
};
UFT_REGISTER_FORMAT_PLUGIN(fdi)
