/**
 * @file uft_td0.c
 * @brief Teledisk (TD0) Format Plugin - API-konform
 */

#include "uft/uft_format_common.h"

#define TD0_MAGIC_NORMAL    0x5444
#define TD0_MAGIC_ADVANCED  0x6474
#define TD0_HEADER_SIZE     12

typedef struct {
    FILE*       file;
    uint8_t     version;
    uint8_t     data_rate;
    uint8_t     sides;
    bool        compressed;
    long        data_start;
} td0_data_t;

static const uint16_t td0_sector_sizes[8] = { 128, 256, 512, 1024, 2048, 4096, 8192, 16384 };

bool td0_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size < 2) return false;
    uint16_t magic = uft_read_le16(data);
    if (magic == TD0_MAGIC_NORMAL || magic == TD0_MAGIC_ADVANCED) {
        *confidence = 95;
        return true;
    }
    return false;
}

static uft_error_t td0_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    uint8_t header[TD0_HEADER_SIZE];
    if (fread(header, 1, TD0_HEADER_SIZE, f) != TD0_HEADER_SIZE) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    uint16_t magic = uft_read_le16(header);
    if (magic != TD0_MAGIC_NORMAL && magic != TD0_MAGIC_ADVANCED) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    td0_data_t* pdata = calloc(1, sizeof(td0_data_t));
    if (!pdata) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    
    pdata->file = f;
    pdata->version = header[4];
    pdata->data_rate = header[5];
    pdata->sides = header[9];
    pdata->compressed = (magic == TD0_MAGIC_ADVANCED);
    
    // Skip comment if present
    if (pdata->version >= 0x10) {
        uint8_t com_hdr[10];
        if (fread(com_hdr, 1, 10, f) != 10) { free(pdata); fclose(f); return UFT_ERROR_IO; }
        uint16_t com_len = uft_read_le16(com_hdr + 2);
        if (fseek(f, com_len, SEEK_CUR) != 0) {
            free(pdata);
            fclose(f);
            return UFT_ERROR_FILE_READ;
        }
    }
    pdata->data_start = ftell(f);
    
    // Scan for geometry
    uint8_t max_cyl = 0, max_sec = 0;
    while (!feof(f)) {
        uint8_t trk_hdr[4];
        if (fread(trk_hdr, 1, 4, f) != 4) break;
        if (trk_hdr[0] == 0xFF) break;
        
        uint8_t num_sec = trk_hdr[0], cyl = trk_hdr[1];
        if (cyl > max_cyl) max_cyl = cyl;
        if (num_sec > max_sec) max_sec = num_sec;
        
        for (int s = 0; s < num_sec; s++) {
            uint8_t sec_hdr[6];
            if (fread(sec_hdr, 1, 6, f) != 6) { break; }
            if (!(sec_hdr[4] & 0x30)) {
                uint8_t len_buf[2];
                if (fread(len_buf, 1, 2, f) != 2) { break; }
                uint16_t len = uft_read_le16(len_buf);
                if (fseek(f, len - 1, SEEK_CUR) != 0) {
                    free(pdata);
                    fclose(f);
                    return UFT_ERROR_FILE_READ;
                }
            }
        }
    }
    
    disk->plugin_data = pdata;
    disk->geometry.cylinders = max_cyl + 1;
    disk->geometry.heads = pdata->sides;
    disk->geometry.sectors = max_sec;
    disk->geometry.sector_size = 512;
    disk->geometry.total_sectors = (uint32_t)(max_cyl + 1) * pdata->sides * max_sec;

    return UFT_OK;
}

static void td0_close(uft_disk_t* disk) {
    td0_data_t* pdata = disk->plugin_data;
    if (pdata) {
        if (pdata->file) fclose(pdata->file);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

static uft_error_t td0_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track) {
    td0_data_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERR_INVALID_ARG;

    uft_track_init(track, cyl, head);

    /* Seek to data start and scan forward to the requested track */
    if (fseek(p->file, p->data_start, SEEK_SET) != 0)
        return UFT_ERR_IO;

    while (!feof(p->file)) {
        uint8_t trk_hdr[4];
        if (fread(trk_hdr, 1, 4, p->file) != 4) break;
        if (trk_hdr[0] == 0xFF) break;  /* End marker */

        uint8_t num_sec = trk_hdr[0];
        uint8_t trk_cyl = trk_hdr[1];
        uint8_t trk_head = trk_hdr[2];
        /* trk_hdr[3] = CRC */

        bool is_target = (trk_cyl == cyl && trk_head == head);

        for (int s = 0; s < num_sec; s++) {
            uint8_t sec_hdr[6];
            if (fread(sec_hdr, 1, 6, p->file) != 6) goto done;

            uint8_t sec_cyl = sec_hdr[0];
            uint8_t sec_head = sec_hdr[1];
            uint8_t sec_num = sec_hdr[2];
            uint8_t sec_size_code = sec_hdr[3];
            uint8_t sec_flags = sec_hdr[4];

            uint16_t sec_size = (sec_size_code < 7) ? (128 << sec_size_code) : 512;

            if (sec_flags & 0x30) {
                /* No data for this sector */
                if (is_target) {
                    uft_format_add_empty_sector(track, sec_num > 0 ? sec_num - 1 : 0,
                                                 (uint16_t)sec_size, 0xE5,
                                                 (uint8_t)cyl, (uint8_t)head);
                    if (track->sector_count > 0) {
                        if (sec_flags & 0x01)
                            track->sectors[track->sector_count - 1].crc_ok = false;
                        if (sec_flags & 0x04)
                            track->sectors[track->sector_count - 1].deleted = true;
                    }
                }
                continue;
            }

            /* Read data length */
            uint8_t len_buf[2];
            if (fread(len_buf, 1, 2, p->file) != 2) goto done;
            uint16_t data_len = uft_read_le16(len_buf);

            if (is_target && data_len > 0) {
                /* Read and decode sector data */
                uint8_t *raw = malloc(data_len);
                if (!raw) goto done;
                if (fread(raw, 1, data_len, p->file) != data_len) {
                    free(raw); goto done;
                }

                /* TD0 encoding: byte 0 = method (0=raw, 1=repeat, 2=pattern) */
                uint8_t *decoded = calloc(1, sec_size);
                if (decoded) {
                    if (raw[0] == 0 && data_len > 1) {
                        /* Raw data */
                        size_t cp = (data_len - 1 < sec_size) ? data_len - 1 : sec_size;
                        memcpy(decoded, raw + 1, cp);
                    } else if (raw[0] == 1 && data_len >= 5) {
                        /* Repeat: 2-byte count + 2-byte pattern */
                        uint16_t count = uft_read_le16(raw + 1);
                        uint8_t p0 = raw[3], p1 = raw[4];
                        for (uint16_t i = 0; i < count && i * 2 + 1 < sec_size; i++) {
                            decoded[i * 2] = p0;
                            decoded[i * 2 + 1] = p1;
                        }
                    } else if (raw[0] == 2 && data_len > 1) {
                        /* Pattern blocks */
                        size_t sp = 1, dp = 0;
                        while (sp < data_len && dp < sec_size) {
                            if (sp + 1 >= data_len) break;
                            uint8_t type = raw[sp++];
                            uint8_t count = raw[sp++];
                            if (type == 0) {
                                /* Literal bytes */
                                for (int i = 0; i < count && sp < data_len && dp < sec_size; i++)
                                    decoded[dp++] = raw[sp++];
                            } else {
                                /* Repeat pattern */
                                size_t pat_start = sp;
                                sp += type;
                                for (int i = 0; i < count; i++)
                                    for (int j = 0; j < type && dp < sec_size; j++)
                                        decoded[dp++] = raw[pat_start + j];
                            }
                        }
                    }

                    uft_format_add_sector(track, sec_num > 0 ? sec_num - 1 : 0,
                                          decoded, (uint16_t)sec_size,
                                          (uint8_t)cyl, (uint8_t)head);
                    /* Propagate TD0 sector flags */
                    if (track->sector_count > 0) {
                        if (sec_flags & 0x01)
                            track->sectors[track->sector_count - 1].crc_ok = false;
                        if (sec_flags & 0x04)
                            track->sectors[track->sector_count - 1].deleted = true;
                    }
                    free(decoded);
                }
                free(raw);
            } else {
                /* Skip data */
                if (data_len > 1) {
                    if (fseek(p->file, data_len, SEEK_CUR) != 0) goto done;
                }
            }
        }

        if (is_target) break;  /* Found our track, done */
    }
done:
    return UFT_OK;
}

/* NOTE: write_track omitted by design — TD0 uses LZSS (Teledisk's own
 * LZ-Huffman variant) plus per-sector RLE. Writing a track requires
 * re-compressing the whole image as a stream, which cannot be done
 * track-by-track via the plugin interface. Use a dedicated TD0 writer
 * if round-trip is required. */
const uft_format_plugin_t uft_format_plugin_td0 = {
    .name = "TD0",
    .description = "Teledisk Archive",
    .extensions = "td0",
    .version = 0x00010000,
    .format = UFT_FORMAT_TD0,
    .capabilities = UFT_FORMAT_CAP_READ,
    .probe = td0_probe,
    .open = td0_open,
    .close = td0_close,
    .read_track = td0_read_track,
};

UFT_REGISTER_FORMAT_PLUGIN(td0)
