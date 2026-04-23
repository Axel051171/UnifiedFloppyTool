/**
 * @file uft_savedskf.c
 * @brief IBM OS/2 SaveDskF (Disk Save Format) parser
 *
 * Reads SaveDskF files with support for uncompressed and RLE-compressed
 * disk images. LZSS decompression is noted as TODO.
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
#include "uft/formats/pc/uft_savedskf.h"

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static uint16_t read_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/* ============================================================================
 * RLE Decompression
 * ============================================================================ */

/**
 * SaveDskF RLE format:
 * - If byte != 0x00: literal byte
 * - If byte == 0x00: escape sequence:
 *   - Next byte = count
 *   - If count == 0: end of data
 *   - Next byte = value to repeat 'count' times
 */
int uft_savedskf_decompress_rle(const uint8_t *src, size_t src_len,
                                uint8_t *dst, size_t dst_len,
                                size_t *out_len)
{
    if (!src || !dst) return UFT_E_INVALID_ARG;

    size_t si = 0;  /* source index */
    size_t di = 0;  /* destination index */

    while (si < src_len && di < dst_len) {
        uint8_t byte = src[si++];

        if (byte != UFT_SAVEDSKF_RLE_ESCAPE) {
            /* Literal byte */
            dst[di++] = byte;
        } else {
            /* Escape sequence */
            if (si >= src_len) break;
            uint8_t count = src[si++];

            if (count == 0) break;  /* End of data marker */

            if (si >= src_len) break;
            uint8_t value = src[si++];

            /* Repeat value 'count' times */
            for (uint8_t r = 0; r < count && di < dst_len; r++) {
                dst[di++] = value;
            }
        }
    }

    if (out_len) *out_len = di;
    return 0;
}

/* ============================================================================
 * API Implementation
 * ============================================================================ */

void uft_savedskf_image_init(uft_savedskf_image_t *image)
{
    if (!image) return;
    memset(image, 0, sizeof(*image));
}

void uft_savedskf_image_free(uft_savedskf_image_t *image)
{
    if (!image) return;
    free(image->sector_data);
    memset(image, 0, sizeof(*image));
}

bool uft_savedskf_probe(const uint8_t *data, size_t size, int *confidence)
{
    if (!data || size < UFT_SAVEDSKF_HEADER_SIZE) return false;

    uint16_t magic = read_le16(data);
    if (magic != UFT_SAVEDSKF_MAGIC) return false;

    /* Additional validation: check geometry values */
    uint16_t sec_size = read_le16(&data[2]);
    uint16_t spt = read_le16(&data[4]);
    uint16_t heads = read_le16(&data[6]);
    uint16_t cyls = read_le16(&data[8]);
    uint8_t  comp = data[10];

    /* Sanity checks */
    if (sec_size != 128 && sec_size != 256 && sec_size != 512 &&
        sec_size != 1024 && sec_size != 2048 && sec_size != 4096) {
        return false;
    }
    if (spt == 0 || spt > UFT_SAVEDSKF_MAX_SECTORS) return false;
    if (heads == 0 || heads > UFT_SAVEDSKF_MAX_HEADS) return false;
    if (cyls == 0 || cyls > UFT_SAVEDSKF_MAX_CYLINDERS) return false;
    if (comp > 2) return false;

    if (confidence) *confidence = 90;
    return true;
}

const char *uft_savedskf_compression_name(uint8_t method)
{
    switch (method) {
        case UFT_SAVEDSKF_COMP_NONE: return "None";
        case UFT_SAVEDSKF_COMP_RLE:  return "RLE";
        case UFT_SAVEDSKF_COMP_LZSS: return "LZSS";
        default: return "Unknown";
    }
}

int uft_savedskf_read(const char *path, uft_savedskf_image_t *image,
                      uft_savedskf_read_result_t *result)
{
    if (!path || !image) return UFT_E_INVALID_ARG;

    uft_savedskf_image_init(image);

    FILE *fp = fopen(path, "rb");
    if (!fp) return UFT_E_FILE_NOT_FOUND;

    /* Get file size */
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return UFT_E_IO;
    }
    long file_size = ftell(fp);
    if (file_size < 0 || (size_t)file_size < UFT_SAVEDSKF_HEADER_SIZE) {
        fclose(fp);
        return UFT_E_FILE_TOO_SMALL;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return UFT_E_IO;
    }

    /* Read header */
    uint8_t hdr_buf[UFT_SAVEDSKF_HEADER_SIZE];
    if (fread(hdr_buf, 1, UFT_SAVEDSKF_HEADER_SIZE, fp)
            != UFT_SAVEDSKF_HEADER_SIZE) {
        fclose(fp);
        return UFT_E_FILE_READ;
    }

    /* Verify magic */
    uint16_t magic = read_le16(hdr_buf);
    if (magic != UFT_SAVEDSKF_MAGIC) {
        fclose(fp);
        return UFT_E_MAGIC;
    }

    /* Parse header */
    image->header.magic = magic;
    image->header.sector_size = read_le16(&hdr_buf[2]);
    image->header.sectors_per_track = read_le16(&hdr_buf[4]);
    image->header.heads = read_le16(&hdr_buf[6]);
    image->header.cylinders = read_le16(&hdr_buf[8]);
    image->header.compression = hdr_buf[10];

    /* Validate geometry */
    if (image->header.sector_size == 0 ||
        image->header.sectors_per_track == 0 ||
        image->header.heads == 0 ||
        image->header.cylinders == 0) {
        fclose(fp);
        return UFT_E_FORMAT_INVALID;
    }

    if (image->header.heads > UFT_SAVEDSKF_MAX_HEADS ||
        image->header.cylinders > UFT_SAVEDSKF_MAX_CYLINDERS ||
        image->header.sectors_per_track > UFT_SAVEDSKF_MAX_SECTORS) {
        fclose(fp);
        return UFT_E_FORMAT_INVALID;
    }

    /* Calculate disk size */
    image->total_sectors = (uint32_t)image->header.cylinders *
                           image->header.heads *
                           image->header.sectors_per_track;
    image->disk_size = image->total_sectors * image->header.sector_size;

    /* Protect against unreasonable sizes (max 16 MB for floppies) */
    if (image->disk_size > 16 * 1024 * 1024) {
        fclose(fp);
        return UFT_E_FILE_TOO_LARGE;
    }

    /* Allocate sector data buffer */
    image->sector_data = malloc(image->disk_size);
    if (!image->sector_data) {
        fclose(fp);
        return UFT_E_MEMORY;
    }

    /* Read and decompress data */
    size_t compressed_size = (size_t)(file_size - UFT_SAVEDSKF_HEADER_SIZE);
    int rc = 0;

    switch (image->header.compression) {
        case UFT_SAVEDSKF_COMP_NONE: {
            /* Uncompressed: read directly */
            size_t to_read = image->disk_size;
            if (to_read > compressed_size) to_read = compressed_size;

            if (fread(image->sector_data, 1, to_read, fp) != to_read) {
                rc = UFT_E_FILE_READ;
            }
            image->sector_data_size = to_read;
            break;
        }

        case UFT_SAVEDSKF_COMP_RLE: {
            /* RLE compressed: read compressed data, then decompress */
            uint8_t *comp_data = malloc(compressed_size);
            if (!comp_data) {
                rc = UFT_E_MEMORY;
                break;
            }

            if (fread(comp_data, 1, compressed_size, fp) != compressed_size) {
                free(comp_data);
                rc = UFT_E_FILE_READ;
                break;
            }

            size_t decompressed_len = 0;
            rc = uft_savedskf_decompress_rle(comp_data, compressed_size,
                                             image->sector_data,
                                             image->disk_size,
                                             &decompressed_len);
            free(comp_data);

            if (rc == 0) {
                image->sector_data_size = decompressed_len;
            }
            break;
        }

        case UFT_SAVEDSKF_COMP_LZSS: {
            /* LZSS compression: not yet implemented */
            rc = UFT_E_NOT_IMPLEMENTED;
            break;
        }

        default:
            rc = UFT_E_FORMAT_UNSUPPORTED;
            break;
    }

    fclose(fp);

    if (rc != 0) {
        uft_savedskf_image_free(image);
        return rc;
    }

    image->is_open = true;

    /* Fill result */
    if (result) {
        memset(result, 0, sizeof(*result));
        result->success = true;
        result->error_code = 0;
        result->cylinders = image->header.cylinders;
        result->heads = image->header.heads;
        result->sectors_per_track = image->header.sectors_per_track;
        result->sector_size = image->header.sector_size;
        result->compression = image->header.compression;
        result->compressed_size = compressed_size;
        result->uncompressed_size = image->sector_data_size;
    }

    return 0;
}

int uft_savedskf_read_sector(const uft_savedskf_image_t *image,
                             uint16_t cyl, uint8_t head, uint16_t sector,
                             uint8_t *buffer, size_t size)
{
    if (!image || !buffer || !image->sector_data) return UFT_E_INVALID_ARG;

    uint16_t sec_size = image->header.sector_size;
    if (sec_size == 0) return UFT_E_FORMAT;
    if (size < sec_size) return UFT_E_BUFFER_TOO_SMALL;

    uint16_t spt = image->header.sectors_per_track;
    uint16_t heads = image->header.heads;
    uint16_t cyls = image->header.cylinders;

    /* Sector is 1-based */
    if (sector < 1 || sector > spt) return UFT_E_RANGE;
    if (head >= heads) return UFT_E_RANGE;
    if (cyl >= cyls) return UFT_E_RANGE;

    /* CHS -> LBA */
    uint32_t lba = ((uint32_t)cyl * heads + head) * spt + (sector - 1);
    size_t offset = (size_t)lba * sec_size;

    if (offset + sec_size > image->sector_data_size) {
        return UFT_E_RANGE;
    }

    memcpy(buffer, image->sector_data + offset, sec_size);
    return (int)sec_size;
}

int uft_savedskf_to_img(const char *dskf_path, const char *img_path)
{
    if (!dskf_path || !img_path) return UFT_E_INVALID_ARG;

    uft_savedskf_image_t image;
    int rc = uft_savedskf_read(dskf_path, &image, NULL);
    if (rc != 0) return rc;

    FILE *fp = fopen(img_path, "wb");
    if (!fp) {
        uft_savedskf_image_free(&image);
        return UFT_E_FILE_OPEN;
    }

    if (image.sector_data && image.sector_data_size > 0) {
        fwrite(image.sector_data, 1, image.sector_data_size, fp);
    }

    if (ferror(fp)) {
        fclose(fp);
        uft_savedskf_image_free(&image);
        return UFT_E_IO;
    }

    fclose(fp);
    uft_savedskf_image_free(&image);
    return 0;
}
