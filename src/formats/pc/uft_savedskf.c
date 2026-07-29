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

/* SaveDskF stores its signature big-endian; geometry fields are little-endian. */
static uint16_t read_be16(const uint8_t *p) {
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

/* Classify the big-endian signature. Returns true + sets *comp / *old for a
 * known signature, false otherwise. */
static bool savedskf_classify(uint16_t sig, uint8_t *comp, bool *old) {
    switch (sig) {
        case UFT_SAVEDSKF_MAGIC_OLD: *comp = UFT_SAVEDSKF_COMP_NONE; *old = true;  return true;
        case UFT_SAVEDSKF_MAGIC_NEW: *comp = UFT_SAVEDSKF_COMP_NONE; *old = false; return true;
        case UFT_SAVEDSKF_MAGIC_LZW: *comp = UFT_SAVEDSKF_COMP_LZW;  *old = false; return true;
        default: return false;
    }
}

/* NOTE: the prior 1.0.0 reader carried a SaveDskF "RLE" decompressor. Real
 * SaveDskF has no RLE mode — its only compression is IBM-LZW (magic 0xAA5A).
 * The RLE path was part of the same fabricated spec and has been removed
 * (MF-356). Compressed images are handled by the LZW branch in the reader. */

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

    uint8_t comp; bool old;
    if (!savedskf_classify(read_be16(data), &comp, &old)) return false;

    /* Geometry lives at authoritative little-endian offsets (deark layout). */
    uint16_t sec_size = read_le16(&data[UFT_SAVEDSKF_OFF_SECSIZE]);
    uint16_t cyls     = read_le16(&data[UFT_SAVEDSKF_OFF_CYLINDERS]);
    uint16_t heads    = read_le16(&data[UFT_SAVEDSKF_OFF_HEADS]);
    uint16_t spt      = read_le16(&data[UFT_SAVEDSKF_OFF_SPT]);

    if (sec_size != 128 && sec_size != 256 && sec_size != 512 &&
        sec_size != 1024 && sec_size != 2048 && sec_size != 4096) {
        return false;
    }
    if (spt == 0 || spt > UFT_SAVEDSKF_MAX_SECTORS) return false;
    if (heads == 0 || heads > UFT_SAVEDSKF_MAX_HEADS) return false;
    if (cyls < 20 || cyls > UFT_SAVEDSKF_MAX_CYLINDERS) return false;

    if (confidence) *confidence = 95;   /* BE signature is highly specific */
    return true;
}

const char *uft_savedskf_compression_name(uint8_t method)
{
    switch (method) {
        case UFT_SAVEDSKF_COMP_NONE: return "None";
        case UFT_SAVEDSKF_COMP_LZW:  return "IBM-LZW";
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

    /* Verify big-endian signature and classify (old/new/compressed). */
    uint16_t magic = read_be16(hdr_buf);
    uint8_t  comp;  bool old_fmt;
    if (!savedskf_classify(magic, &comp, &old_fmt)) {
        fclose(fp);
        return UFT_E_MAGIC;
    }

    /* Parse header at authoritative offsets (deark loaddskf_read_header). */
    image->header.magic = magic;
    image->header.sector_size       = read_le16(&hdr_buf[UFT_SAVEDSKF_OFF_SECSIZE]);
    image->header.cylinders         = read_le16(&hdr_buf[UFT_SAVEDSKF_OFF_CYLINDERS]);
    image->header.heads             = read_le16(&hdr_buf[UFT_SAVEDSKF_OFF_HEADS]);
    image->header.sectors_per_track = read_le16(&hdr_buf[UFT_SAVEDSKF_OFF_SPT]);
    image->header.num_sectors_in_image = read_le16(&hdr_buf[UFT_SAVEDSKF_OFF_NUMSECS]);
    image->header.header_size       = read_le16(&hdr_buf[UFT_SAVEDSKF_OFF_HDRSIZE]);
    image->header.compression       = comp;

    /* Validate geometry (deark sanity ranges). */
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

    /* Sector data offset: old fmt is fixed at 0x200; new fmt uses the
     * header_size field (0 => 512). */
    size_t data_off = old_fmt ? UFT_SAVEDSKF_OLD_DATA_OFF
                              : (image->header.header_size ? image->header.header_size : 512);

    /* Full-geometry image size. */
    image->total_sectors = (uint32_t)image->header.cylinders *
                           image->header.heads *
                           image->header.sectors_per_track;
    image->disk_size = image->total_sectors * image->header.sector_size;
    if (image->disk_size == 0 || image->disk_size > 16 * 1024 * 1024) {
        fclose(fp);
        return UFT_E_FILE_TOO_LARGE;
    }

    /* calloc: sectors not physically stored (SaveDskF omits unused sectors)
     * are reconstructed as empty — the format's defined meaning, not
     * fabricated data. */
    image->sector_data = calloc(1, image->disk_size);
    if (!image->sector_data) {
        fclose(fp);
        return UFT_E_MEMORY;
    }

    int rc = 0;
    if (comp == UFT_SAVEDSKF_COMP_LZW) {
        /* IBM-LZW ("ibmlzw", dskdcmps): faithful port pending a ground-truth
         * compressed reference file for byte-exact verification. Porting a
         * ~200-line LZW codec unverified would risk silent corruption, which
         * the forensic mandate forbids. See docs/KNOWN_ISSUES.md. */
        rc = UFT_E_NOT_IMPLEMENTED;
    } else {
        /* Uncompressed: num_sectors_in_image sectors are stored contiguously at
         * data_off; copy them into the zero-filled full-geometry buffer. */
        size_t stored = (size_t)image->header.num_sectors_in_image *
                        image->header.sector_size;
        if (stored == 0 || stored > image->disk_size) stored = image->disk_size;
        size_t avail = (size_t)file_size > data_off ? (size_t)file_size - data_off : 0;
        if (stored > avail) stored = avail;

        if (stored > 0) {
            if (fseek(fp, (long)data_off, SEEK_SET) != 0 ||
                fread(image->sector_data, 1, stored, fp) != stored) {
                rc = UFT_E_FILE_READ;
            }
        }
        image->sector_data_size = image->disk_size;
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
        result->compressed_size = (size_t)file_size;
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
