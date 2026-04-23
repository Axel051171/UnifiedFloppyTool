/**
 * @file uft_aaru.c
 * @brief Aaru (DICF - Disc Image Chef Format) read-only parser
 *
 * Minimal implementation: reads the fixed binary header fields and
 * uncompressed sector data. Complex Aaru files with protobuf metadata
 * or LZMA/FLAC/LZ4 compression are detected but not fully decoded.
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
#include "uft/formats/modern/uft_aaru.h"

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

static uint64_t read_le64(const uint8_t *p) {
    return (uint64_t)read_le32(p) | ((uint64_t)read_le32(p + 4) << 32);
}

/* ============================================================================
 * API Implementation
 * ============================================================================ */

void uft_aaru_image_init(uft_aaru_image_t *image)
{
    if (!image) return;
    memset(image, 0, sizeof(*image));
}

void uft_aaru_image_free(uft_aaru_image_t *image)
{
    if (!image) return;
    free(image->sector_data);
    memset(image, 0, sizeof(*image));
}

bool uft_aaru_probe(const uint8_t *data, size_t size, int *confidence)
{
    if (!data || size < UFT_AARU_MAGIC_LEN) return false;

    if (memcmp(data, UFT_AARU_MAGIC, UFT_AARU_MAGIC_LEN) == 0) {
        if (confidence) *confidence = 98;
        return true;
    }
    return false;
}

const char *uft_aaru_media_type_name(uint16_t media_type)
{
    switch (media_type) {
        case UFT_AARU_MEDIA_FLOPPY_525_DD: return "5.25\" DD Floppy";
        case UFT_AARU_MEDIA_FLOPPY_525_HD: return "5.25\" HD Floppy";
        case UFT_AARU_MEDIA_FLOPPY_35_DD:  return "3.5\" DD Floppy";
        case UFT_AARU_MEDIA_FLOPPY_35_HD:  return "3.5\" HD Floppy";
        case UFT_AARU_MEDIA_FLOPPY_35_ED:  return "3.5\" ED Floppy";
        default: return "Unknown Media";
    }
}

int uft_aaru_read(const char *path, uft_aaru_image_t *image,
                  uft_aaru_read_result_t *result)
{
    if (!path || !image) return UFT_E_INVALID_ARG;

    uft_aaru_image_init(image);

    FILE *fp = fopen(path, "rb");
    if (!fp) return UFT_E_FILE_NOT_FOUND;

    /* Get file size */
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return UFT_E_IO;
    }
    long file_size = ftell(fp);
    if (file_size < 0 || (size_t)file_size < sizeof(uft_aaru_file_header_t)) {
        fclose(fp);
        return UFT_E_FILE_TOO_SMALL;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return UFT_E_IO;
    }

    /* Read file header */
    uint8_t hdr_buf[32];
    if (fread(hdr_buf, 1, sizeof(uft_aaru_file_header_t), fp)
            != sizeof(uft_aaru_file_header_t)) {
        fclose(fp);
        return UFT_E_FILE_READ;
    }

    /* Verify magic */
    if (memcmp(hdr_buf, UFT_AARU_MAGIC, UFT_AARU_MAGIC_LEN) != 0) {
        fclose(fp);
        return UFT_E_MAGIC;
    }

    /* Parse file header */
    memcpy(image->file_header.magic, hdr_buf, 5);
    image->file_header.major_version = hdr_buf[5];
    image->file_header.minor_version = hdr_buf[6];
    image->file_header.flags = hdr_buf[7];
    image->file_header.media_type = read_le16(&hdr_buf[8]);
    image->file_header.creation_time = read_le64(&hdr_buf[10]);
    image->file_header.last_modified = read_le64(&hdr_buf[18]);

    /* Walk block headers looking for media info and sector data */
    bool found_media_info = false;
    bool found_sector_data = false;
    image->partial_parse = false;

    while (!feof(fp) && ftell(fp) < file_size) {
        uint8_t blk_buf[sizeof(uft_aaru_block_header_t)];
        if (fread(blk_buf, 1, sizeof(uft_aaru_block_header_t), fp)
                != sizeof(uft_aaru_block_header_t)) {
            break;  /* End of file or read error - not fatal */
        }

        uft_aaru_block_header_t blk;
        blk.block_type = blk_buf[0];
        blk.compression = blk_buf[1];
        blk.compressed_length = read_le32(&blk_buf[2]);
        blk.uncompressed_length = read_le32(&blk_buf[6]);
        blk.crc32 = read_le32(&blk_buf[10]);

        /* Sanity check block length */
        if (blk.compressed_length > (uint32_t)(file_size - ftell(fp))) {
            break;  /* Block extends past EOF */
        }

        if (blk.block_type == UFT_AARU_BLOCK_MEDIA_INFO &&
            blk.compression == UFT_AARU_COMPRESS_NONE) {
            /* Parse media info from uncompressed data */
            if (blk.uncompressed_length >= sizeof(uft_aaru_media_info_t)) {
                uint8_t mi_buf[sizeof(uft_aaru_media_info_t)];
                if (fread(mi_buf, 1, sizeof(mi_buf), fp) == sizeof(mi_buf)) {
                    image->media_info.media_type = read_le16(&mi_buf[0]);
                    image->media_info.cylinders = read_le16(&mi_buf[2]);
                    image->media_info.heads = mi_buf[4];
                    image->media_info.sectors_per_track = read_le16(&mi_buf[5]);
                    image->media_info.sector_size = read_le16(&mi_buf[7]);
                    image->media_info.total_sectors = read_le32(&mi_buf[9]);
                    image->media_info.encoding = mi_buf[13];
                    image->media_info.data_rate = read_le32(&mi_buf[14]);
                    found_media_info = true;

                    /* Skip remainder of block if any */
                    long remaining = (long)blk.compressed_length
                                     - (long)sizeof(mi_buf);
                    if (remaining > 0) {
                        if (fseek(fp, remaining, SEEK_CUR) != 0) break;
                    }
                    continue;
                }
            }
            /* Could not parse - skip block */
            if (fseek(fp, (long)blk.compressed_length, SEEK_CUR) != 0) break;

        } else if (blk.block_type == UFT_AARU_BLOCK_SECTOR_DATA &&
                   blk.compression == UFT_AARU_COMPRESS_NONE) {
            /* Read uncompressed sector data */
            if (blk.uncompressed_length > 0 &&
                blk.uncompressed_length <= 16 * 1024 * 1024) {
                image->sector_data = malloc(blk.uncompressed_length);
                if (image->sector_data) {
                    size_t rd = fread(image->sector_data, 1,
                                      blk.uncompressed_length, fp);
                    if (rd == blk.uncompressed_length) {
                        image->sector_data_size = rd;
                        found_sector_data = true;
                    } else {
                        free(image->sector_data);
                        image->sector_data = NULL;
                    }
                    continue;
                }
            }
            /* Skip if allocation failed or too large */
            if (fseek(fp, (long)blk.compressed_length, SEEK_CUR) != 0) break;

        } else if (blk.compression != UFT_AARU_COMPRESS_NONE) {
            /* Compressed block - skip with note */
            image->partial_parse = true;
            if (fseek(fp, (long)blk.compressed_length, SEEK_CUR) != 0) break;
        } else {
            /* Other block type - skip */
            if (fseek(fp, (long)blk.compressed_length, SEEK_CUR) != 0) break;
        }
    }

    fclose(fp);

    /* Validate we got at least the media info */
    if (!found_media_info) {
        /* Try to infer geometry from sector data size */
        if (found_sector_data && image->sector_data_size > 0) {
            /* Common floppy sizes */
            if (image->sector_data_size == 1474560) {
                image->media_info.cylinders = 80;
                image->media_info.heads = 2;
                image->media_info.sectors_per_track = 18;
                image->media_info.sector_size = 512;
                image->media_info.total_sectors = 2880;
                found_media_info = true;
            } else if (image->sector_data_size == 737280) {
                image->media_info.cylinders = 80;
                image->media_info.heads = 2;
                image->media_info.sectors_per_track = 9;
                image->media_info.sector_size = 512;
                image->media_info.total_sectors = 1440;
                found_media_info = true;
            }
        }
    }

    image->is_open = true;

    /* Fill result */
    if (result) {
        memset(result, 0, sizeof(*result));
        result->success = found_media_info;
        result->error_code = found_media_info ? 0 : UFT_E_INCOMPLETE;
        result->error_detail = found_media_info ? NULL :
            "Aaru media info not found or protobuf-encoded; "
            "full support requires libAaru";

        result->cylinders = image->media_info.cylinders;
        result->heads = image->media_info.heads;
        result->sectors_per_track = image->media_info.sectors_per_track;
        result->sector_size = image->media_info.sector_size;
        result->total_sectors = image->media_info.total_sectors;

        result->format_major = image->file_header.major_version;
        result->format_minor = image->file_header.minor_version;
        result->media_type = image->file_header.media_type;
        result->has_full_data = found_sector_data && !image->partial_parse;
    }

    return found_media_info ? 0 : UFT_E_INCOMPLETE;
}

int uft_aaru_read_sector(const uft_aaru_image_t *image,
                         uint16_t cyl, uint8_t head, uint16_t sector,
                         uint8_t *buffer, size_t size)
{
    if (!image || !buffer || !image->sector_data) return UFT_E_INVALID_ARG;

    uint16_t sec_size = image->media_info.sector_size;
    if (sec_size == 0) sec_size = 512;
    if (size < sec_size) return UFT_E_BUFFER_TOO_SMALL;

    uint16_t spt = image->media_info.sectors_per_track;
    uint8_t  heads = image->media_info.heads;
    if (spt == 0 || heads == 0) return UFT_E_FORMAT;

    /* Sector is 1-based */
    if (sector < 1 || sector > spt) return UFT_E_RANGE;
    if (head >= heads) return UFT_E_RANGE;
    if (cyl >= image->media_info.cylinders) return UFT_E_RANGE;

    /* Calculate linear offset: CHS -> LBA */
    uint32_t lba = ((uint32_t)cyl * heads + head) * spt + (sector - 1);
    size_t offset = (size_t)lba * sec_size;

    if (offset + sec_size > image->sector_data_size) {
        return UFT_E_RANGE;
    }

    memcpy(buffer, image->sector_data + offset, sec_size);
    return (int)sec_size;
}
