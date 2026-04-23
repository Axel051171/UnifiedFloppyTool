/**
 * @file uft_86f.c
 * @brief 86Box 86F Surface Format parser with MFM decode
 *
 * Reads 86F files storing bitcell-level surface data with timing.
 * Parses track headers, reads bitcell streams, and performs basic
 * MFM decoding to extract sector data.
 *
 * @version 1.0.0
 * @date 2026-04-10
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "uft/uft_error.h"
#include "uft/formats/pc/uft_86f.h"

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static uint16_t read_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* ============================================================================
 * MFM Decode Helpers
 * ============================================================================ */

/**
 * @brief Extract a bit from bitcell data
 */
static int get_bit(const uint8_t *data, size_t bit_index)
{
    return (data[bit_index / 8] >> (7 - (bit_index % 8))) & 1;
}

/**
 * @brief Search for MFM sync pattern (0xA1A1A1) with missing clock
 * In MFM, the sync word 0xA1 with missing clock is:
 *   data bits pattern = 0100010010100100 (repeated 3x)
 * We search for the IDAM marker: 3x A1 + FE
 * Returns bit position after sync, or -1 if not found.
 */
static long find_idam(const uint8_t *data, size_t bit_count, size_t start)
{
    /* Search for 0xFE after three 0xA1 syncs in MFM data bits.
     * Simplified: look for the byte pattern in decoded data.
     * For each candidate position, decode 4 bytes of MFM and check. */
    if (bit_count < start + 128) return -1;

    /* Simple approach: decode MFM clock+data pairs and look for A1,A1,A1,FE */
    for (size_t pos = start; pos + 64 <= bit_count; pos += 2) {
        /* In MFM each data bit is preceded by a clock bit.
         * Data bits are at odd positions (1, 3, 5, ...) */
        uint8_t decoded[4] = {0};
        bool valid = true;

        for (int byte_idx = 0; byte_idx < 4 && valid; byte_idx++) {
            uint8_t val = 0;
            for (int bit = 0; bit < 8; bit++) {
                size_t bp = pos + (size_t)(byte_idx * 16 + bit * 2 + 1);
                if (bp >= bit_count) { valid = false; break; }
                val = (uint8_t)((val << 1) | get_bit(data, bp));
            }
            decoded[byte_idx] = val;
        }

        if (valid &&
            decoded[0] == 0xA1 && decoded[1] == 0xA1 &&
            decoded[2] == 0xA1 && decoded[3] == 0xFE) {
            return (long)(pos + 64);  /* Position after IDAM */
        }
    }
    return -1;
}

/**
 * @brief Decode MFM bytes from bitcell stream
 */
static int decode_mfm_bytes(const uint8_t *data, size_t bit_count,
                            size_t start_bit, uint8_t *output, size_t num_bytes)
{
    for (size_t b = 0; b < num_bytes; b++) {
        uint8_t val = 0;
        for (int bit = 0; bit < 8; bit++) {
            size_t bp = start_bit + (b * 16) + (size_t)(bit * 2 + 1);
            if (bp >= bit_count) return (int)b;
            val = (uint8_t)((val << 1) | get_bit(data, bp));
        }
        output[b] = val;
    }
    return (int)num_bytes;
}

/* ============================================================================
 * API Implementation
 * ============================================================================ */

void uft_pc86f_image_init(uft_pc86f_image_t *image)
{
    if (!image) return;
    memset(image, 0, sizeof(*image));
}

void uft_pc86f_image_free(uft_pc86f_image_t *image)
{
    if (!image) return;

    if (image->tracks) {
        for (size_t t = 0; t < image->track_count; t++) {
            free(image->tracks[t].bitcell_data);
            free(image->tracks[t].surface_data);
        }
        free(image->tracks);
    }
    if (image->decoded_sectors) {
        for (size_t s = 0; s < image->decoded_count; s++) {
            free(image->decoded_sectors[s].data);
        }
        free(image->decoded_sectors);
    }
    memset(image, 0, sizeof(*image));
}

bool uft_pc86f_probe(const uint8_t *data, size_t size, int *confidence)
{
    if (!data || size < sizeof(uft_pc86f_header_t)) return false;

    if (memcmp(data, UFT_PC86F_MAGIC, UFT_PC86F_MAGIC_LEN) == 0) {
        if (confidence) *confidence = 97;
        return true;
    }
    return false;
}

const char *uft_pc86f_disk_type_name(uint8_t disk_type)
{
    switch (disk_type) {
        case UFT_PC86F_TYPE_MFM_DD: return "MFM Double Density";
        case UFT_PC86F_TYPE_MFM_HD: return "MFM High Density";
        case UFT_PC86F_TYPE_MFM_ED: return "MFM Extra Density";
        default: return "Unknown";
    }
}

int uft_pc86f_read(const char *path, uft_pc86f_image_t *image,
                   uft_pc86f_read_result_t *result)
{
    if (!path || !image) return UFT_E_INVALID_ARG;

    uft_pc86f_image_init(image);

    FILE *fp = fopen(path, "rb");
    if (!fp) return UFT_E_FILE_NOT_FOUND;

    /* Get file size */
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return UFT_E_IO;
    }
    long file_size = ftell(fp);
    if (file_size < 0 ||
        (size_t)file_size < sizeof(uft_pc86f_header_t)) {
        fclose(fp);
        return UFT_E_FILE_TOO_SMALL;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return UFT_E_IO;
    }

    /* Read header */
    uint8_t hdr_buf[16];
    if (fread(hdr_buf, 1, sizeof(uft_pc86f_header_t), fp)
            != sizeof(uft_pc86f_header_t)) {
        fclose(fp);
        return UFT_E_FILE_READ;
    }

    /* Verify magic */
    if (memcmp(hdr_buf, UFT_PC86F_MAGIC, UFT_PC86F_MAGIC_LEN) != 0) {
        fclose(fp);
        return UFT_E_MAGIC;
    }

    /* Parse header */
    memcpy(image->header.magic, hdr_buf, 4);
    image->header.version = read_le16(&hdr_buf[4]);
    image->header.flags = read_le16(&hdr_buf[6]);
    image->header.disk_type = hdr_buf[8];
    image->header.encoding = hdr_buf[9];
    image->header.rpm = hdr_buf[10];
    image->header.num_tracks = hdr_buf[11];
    image->header.num_sides = hdr_buf[12];
    image->header.bitcell_mode = hdr_buf[13];

    image->cylinders = image->header.num_tracks;
    image->heads = image->header.num_sides;

    if (image->cylinders == 0 || image->heads == 0 ||
        image->heads > 2 || image->cylinders > 100) {
        fclose(fp);
        return UFT_E_FORMAT_INVALID;
    }

    /* Read track offset table */
    size_t total_tracks = (size_t)image->cylinders * image->heads;
    if (total_tracks > UFT_PC86F_MAX_TRACKS) {
        fclose(fp);
        return UFT_E_FORMAT_INVALID;
    }

    for (size_t i = 0; i < total_tracks && i < UFT_PC86F_MAX_TRACKS; i++) {
        uint8_t off_buf[4];
        if (fread(off_buf, 1, 4, fp) != 4) break;
        image->track_offsets[i] = read_le32(off_buf);
    }

    /* Allocate tracks */
    image->tracks = calloc(total_tracks, sizeof(uft_pc86f_track_t));
    if (!image->tracks) {
        fclose(fp);
        return UFT_E_MEMORY;
    }
    image->track_count = total_tracks;

    bool has_surface = (image->header.flags & UFT_PC86F_FL_HAS_SURFACE) != 0;

    /* Read each track */
    for (size_t i = 0; i < total_tracks; i++) {
        uint32_t offset = image->track_offsets[i];
        if (offset == 0 || (long)offset >= file_size) continue;

        if (fseek(fp, (long)offset, SEEK_SET) != 0) continue;

        /* Read track header */
        uint8_t th_buf[16];
        size_t th_size = sizeof(uft_pc86f_track_header_t);
        if (fread(th_buf, 1, th_size, fp) != th_size) continue;

        uft_pc86f_track_t *track = &image->tracks[i];
        track->header.cylinder = th_buf[0];
        track->header.head = th_buf[1];
        track->header.encoding = th_buf[2];
        track->header.data_rate = th_buf[3];
        track->header.bit_count = read_le32(&th_buf[4]);
        track->header.index_offset = read_le32(&th_buf[8]);
        track->header.num_sectors = read_le16(&th_buf[12]);
        track->header.flags = read_le16(&th_buf[14]);

        /* Read bitcell data */
        uint32_t bit_count = track->header.bit_count;
        if (bit_count == 0 || bit_count > UFT_PC86F_MAX_BITCELLS) continue;

        size_t byte_count = (bit_count + 7) / 8;
        track->bitcell_data = malloc(byte_count);
        if (!track->bitcell_data) continue;

        if (fread(track->bitcell_data, 1, byte_count, fp) != byte_count) {
            free(track->bitcell_data);
            track->bitcell_data = NULL;
            continue;
        }
        track->bitcell_bytes = byte_count;

        /* Read surface data if present */
        if (has_surface) {
            track->surface_data = malloc(byte_count);
            if (track->surface_data) {
                if (fread(track->surface_data, 1, byte_count, fp)
                        != byte_count) {
                    free(track->surface_data);
                    track->surface_data = NULL;
                } else {
                    track->surface_bytes = byte_count;
                }
            }
        }
    }

    fclose(fp);
    image->is_open = true;

    /* Fill result */
    if (result) {
        memset(result, 0, sizeof(*result));
        result->success = true;
        result->error_code = 0;
        result->cylinders = image->cylinders;
        result->heads = image->heads;
        result->disk_type = image->header.disk_type;
        result->encoding = image->header.encoding;
        result->version = image->header.version;
        result->track_count = image->track_count;
        result->has_surface_data = has_surface;
    }

    return 0;
}

const uft_pc86f_track_t *uft_pc86f_get_track(const uft_pc86f_image_t *image,
                                              uint8_t cyl, uint8_t head)
{
    if (!image || !image->tracks) return NULL;
    if (cyl >= image->cylinders || head >= image->heads) return NULL;

    size_t idx = (size_t)cyl * image->heads + head;
    if (idx >= image->track_count) return NULL;

    return &image->tracks[idx];
}

int uft_pc86f_decode_track(const uft_pc86f_image_t *image,
                           uint8_t cyl, uint8_t head,
                           uft_pc86f_decoded_sector_t *sectors,
                           int max_sectors)
{
    if (!image || !sectors || max_sectors <= 0) return UFT_E_INVALID_ARG;

    const uft_pc86f_track_t *track = uft_pc86f_get_track(image, cyl, head);
    if (!track || !track->bitcell_data) return UFT_E_NOT_FOUND;

    int found = 0;
    size_t bit_count = (size_t)track->header.bit_count;
    size_t search_pos = 0;

    while (found < max_sectors && search_pos < bit_count) {
        /* Find next IDAM */
        long idam_pos = find_idam(track->bitcell_data, bit_count, search_pos);
        if (idam_pos < 0) break;

        /* Decode CHRN (4 bytes after IDAM marker) */
        uint8_t chrn[4];
        int decoded = decode_mfm_bytes(track->bitcell_data, bit_count,
                                       (size_t)idam_pos, chrn, 4);
        if (decoded < 4) {
            search_pos = (size_t)idam_pos + 16;
            continue;
        }

        uft_pc86f_decoded_sector_t *sec = &sectors[found];
        memset(sec, 0, sizeof(*sec));
        sec->cylinder = chrn[0];
        sec->head = chrn[1];
        sec->sector = chrn[2];
        sec->size_code = chrn[3];

        /* Calculate sector data size: 128 << N */
        if (sec->size_code > 7) {
            search_pos = (size_t)idam_pos + 80;
            continue;
        }
        size_t sec_size = (size_t)128 << sec->size_code;

        /* Skip CRC (2 bytes) + gap + DAM sync (3xA1 + FB/F8) */
        /* Approximate: CHRN(4*16) + CRC(2*16) + gap(~22*16) + sync(4*16) = 512 bits */
        size_t dam_search = (size_t)idam_pos + 4 * 16;

        /* Look for Data Address Mark: A1 A1 A1 FB or A1 A1 A1 F8 */
        long dam_pos = -1;
        for (size_t p = dam_search; p + 64 <= bit_count && p < dam_search + 1024; p += 2) {
            uint8_t dam_bytes[4];
            if (decode_mfm_bytes(track->bitcell_data, bit_count, p, dam_bytes, 4) == 4) {
                if (dam_bytes[0] == 0xA1 && dam_bytes[1] == 0xA1 &&
                    dam_bytes[2] == 0xA1 &&
                    (dam_bytes[3] == 0xFB || dam_bytes[3] == 0xF8)) {
                    dam_pos = (long)(p + 64);
                    sec->deleted = (dam_bytes[3] == 0xF8);
                    break;
                }
            }
        }

        if (dam_pos < 0) {
            search_pos = (size_t)idam_pos + 80;
            continue;
        }

        /* Decode sector data */
        sec->data = malloc(sec_size);
        if (!sec->data) break;

        decoded = decode_mfm_bytes(track->bitcell_data, bit_count,
                                   (size_t)dam_pos, sec->data, sec_size);
        if (decoded < (int)sec_size) {
            free(sec->data);
            sec->data = NULL;
            search_pos = (size_t)dam_pos + 16;
            continue;
        }
        sec->data_size = sec_size;

        /* Read and verify CRC (2 bytes after sector data) */
        uint8_t crc_bytes[2];
        if (decode_mfm_bytes(track->bitcell_data, bit_count,
                             (size_t)dam_pos + sec_size * 16,
                             crc_bytes, 2) == 2) {
            uint16_t stored_crc = ((uint16_t)crc_bytes[0] << 8) | crc_bytes[1];
            /* CRC-CCITT: init 0xCDB4 (after 3× A1), over DAM + data */
            uint16_t calc = 0xCDB4;
            uint8_t dam_byte = 0xFB;
            calc ^= (uint16_t)dam_byte << 8;
            for (int cj = 0; cj < 8; cj++)
                calc = (calc & 0x8000) ? (calc << 1) ^ 0x1021 : (calc << 1);
            if (sec->data) {
                for (uint16_t ci = 0; ci < sec_size; ci++) {
                    calc ^= (uint16_t)sec->data[ci] << 8;
                    for (int cj = 0; cj < 8; cj++)
                        calc = (calc & 0x8000) ? (calc << 1) ^ 0x1021 : (calc << 1);
                }
            }
            sec->crc_ok = (calc == stored_crc);
        }

        found++;
        search_pos = (size_t)dam_pos + sec_size * 16 + 32;
    }

    return found;
}

int uft_pc86f_to_img(const char *f86_path, const char *img_path)
{
    if (!f86_path || !img_path) return UFT_E_INVALID_ARG;

    uft_pc86f_image_t image;
    int rc = uft_pc86f_read(f86_path, &image, NULL);
    if (rc != 0) return rc;

    FILE *fp = fopen(img_path, "wb");
    if (!fp) {
        uft_pc86f_image_free(&image);
        return UFT_E_FILE_OPEN;
    }

    uft_pc86f_decoded_sector_t sectors[64];

    for (uint8_t cyl = 0; cyl < image.cylinders; cyl++) {
        for (uint8_t head = 0; head < image.heads; head++) {
            memset(sectors, 0, sizeof(sectors));
            int nsec = uft_pc86f_decode_track(&image, cyl, head,
                                              sectors, 64);

            /* Sort sectors by sector number */
            for (int i = 0; i < nsec - 1; i++) {
                for (int j = i + 1; j < nsec; j++) {
                    if (sectors[j].sector < sectors[i].sector) {
                        uft_pc86f_decoded_sector_t tmp = sectors[i];
                        sectors[i] = sectors[j];
                        sectors[j] = tmp;
                    }
                }
            }

            for (int s = 0; s < nsec; s++) {
                if (sectors[s].data && sectors[s].data_size > 0) {
                    fwrite(sectors[s].data, 1, sectors[s].data_size, fp);
                }
                free(sectors[s].data);
                sectors[s].data = NULL;
            }
        }
    }

    fclose(fp);
    uft_pc86f_image_free(&image);
    return 0;
}
