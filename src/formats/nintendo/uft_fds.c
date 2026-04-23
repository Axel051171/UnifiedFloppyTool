/**
 * @file uft_fds.c
 * @brief Famicom Disk System (FDS) Image Parser Implementation
 *
 * Parses FDS disk images in both headered (fwNES "FDS\x1A" prefix)
 * and headerless (raw) formats.
 *
 * Disk layout per side (65500 bytes):
 *   Block 1 (Disk Info):   1 + 55 bytes
 *   Block 2 (File Amount): 1 + 1 bytes
 *   Block 3 (File Header): 1 + 15 bytes (repeated per file)
 *   Block 4 (File Data):   1 + N bytes (repeated per file)
 *
 * @author UFT Project
 * @date 2026-04-10
 */

#include "uft/formats/nintendo/uft_fds.h"
#include "uft/uft_error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

/**
 * @brief Parse disk data (without fwNES header) into image context
 */
static int fds_parse_disk_data(const uint8_t *data, size_t size,
                                uft_fds_image_t *img)
{
    if (size < 58) {
        /* Need at least Block 1 (56 bytes) + Block 2 (2 bytes) */
        return UFT_ERR_CORRUPTED;
    }

    size_t offset = 0;

    /* ---- Block 1: Disk Info (block_type = 0x01) ---- */
    if (data[offset] != FDS_BLOCK_DISK_INFO) {
        /* Try detecting headerless FDS: some dumps omit block type byte
         * and start directly with *NINTENDO-HVC* at offset 1.
         * If the first byte is 0x01 that is the standard block type. */

        /* Check for *NINTENDO-HVC* starting at offset 1 anyway */
        if (size >= 56 && memcmp(data + 1, FDS_NINTENDO_SIG, FDS_NINTENDO_LEN) == 0) {
            /* block type byte is present but we already checked */
        } else {
            return UFT_ERR_CORRUPTED;
        }
    }

    /* Validate *NINTENDO-HVC* signature at offset 1..14 */
    if (memcmp(data + 1, FDS_NINTENDO_SIG, FDS_NINTENDO_LEN) == 0) {
        img->valid_signature = true;
    } else {
        img->valid_signature = false;
    }

    /* Parse disk info fields (offsets relative to start of Block 1 data at +1) */
    const uint8_t *info_data = data + 1; /* skip block type byte */

    /* Manufacturer code at offset 15 (after 14-byte signature + 1 byte) */
    img->manufacturer_code = info_data[15];

    /* Game name: 4 bytes at offset 16 */
    memcpy(img->game_name, info_data + 16, 4);
    img->game_name[4] = '\0';

    /* Revision at offset 20 */
    img->revision = info_data[20];

    /* Side number at offset 21 */
    img->side_number = info_data[21];

    /* Disk number at offset 22 */
    img->disk_number = info_data[22];

    /* Advance past Block 1: 1 (type) + 55 (data) = 56 bytes */
    offset = 56;

    /* ---- Block 2: File Amount (block_type = 0x02) ---- */
    if (offset >= size) {
        img->file_count = 0;
        return 0;
    }

    if (data[offset] != FDS_BLOCK_FILE_AMOUNT) {
        /* Some dumps may have padding or CRC gaps; tolerate gracefully */
        img->file_count = 0;
        return 0;
    }

    offset++; /* skip block type */
    if (offset >= size) {
        img->file_count = 0;
        return 0;
    }

    img->file_count = data[offset];
    offset++;

    /* ---- Parse file entries (Block 3 + Block 4 pairs) ---- */
    int files_parsed = 0;
    int max_files = img->file_count;
    if (max_files > 64) max_files = 64;

    for (int i = 0; i < max_files && offset < size; i++) {
        /* Block 3: File Header */
        if (data[offset] != FDS_BLOCK_FILE_HEADER) break;
        offset++; /* skip block type */

        if (offset + FDS_FILE_HEADER_SIZE > size) break;

        fds_file_entry_t *entry = &img->files[files_parsed];
        entry->file_number  = data[offset + 0];
        entry->file_id      = data[offset + 1];

        /* File name: 8 bytes at offset+2 */
        memcpy(entry->file_name, data + offset + 2, 8);
        entry->file_name[8] = '\0';

        /* Destination address: LE16 at offset+10 */
        entry->dest_address = read_le16(data + offset + 10);

        /* File size: LE16 at offset+12 */
        entry->file_size = read_le16(data + offset + 12);

        /* File type at offset+14 */
        entry->file_type = data[offset + 14];

        offset += FDS_FILE_HEADER_SIZE;

        /* Block 4: File Data */
        if (offset >= size) break;
        if (data[offset] != FDS_BLOCK_FILE_DATA) break;
        offset++; /* skip block type */

        /* Skip file data bytes */
        if (offset + entry->file_size > size) {
            /* Truncated file data; accept what we have */
            offset = size;
        } else {
            offset += entry->file_size;
        }

        files_parsed++;
    }

    img->file_count = files_parsed;
    return 0;
}

/* ============================================================================
 * API Implementation
 * ============================================================================ */

int uft_fds_open_buffer(const uint8_t *data, size_t size, uft_fds_image_t *img)
{
    if (!data || !img) return UFT_ERR_NULL_POINTER;
    memset(img, 0, sizeof(uft_fds_image_t));

    if (size < FDS_DISK_SIZE) {
        /* Too small for even one headerless side */
        return UFT_ERR_CORRUPTED;
    }

    const uint8_t *disk_data = data;
    size_t disk_size = size;

    /* Check for fwNES "FDS\x1A" header */
    if (size >= FDS_HEADER_SIZE && memcmp(data, FDS_MAGIC, 4) == 0) {
        img->has_header = true;
        img->side_count = data[4];
        if (img->side_count == 0) img->side_count = 1;
        if (img->side_count > FDS_SIDES_MAX) img->side_count = FDS_SIDES_MAX;
        disk_data = data + FDS_HEADER_SIZE;
        disk_size = size - FDS_HEADER_SIZE;
    } else {
        img->has_header = false;
        /* Estimate side count from size */
        img->side_count = (int)((disk_size + FDS_DISK_SIZE - 1) / FDS_DISK_SIZE);
        if (img->side_count > FDS_SIDES_MAX) img->side_count = FDS_SIDES_MAX;
    }

    /* Allocate and copy raw disk data */
    img->data = malloc(disk_size);
    if (!img->data) return UFT_ERR_MEMORY;

    memcpy(img->data, disk_data, disk_size);
    img->data_size = disk_size;

    /* Parse the first side's block structure */
    return fds_parse_disk_data(disk_data, disk_size, img);
}

int uft_fds_open(const char *path, uft_fds_image_t *img)
{
    if (!path || !img) return UFT_ERR_NULL_POINTER;

    FILE *fp = fopen(path, "rb");
    if (!fp) return UFT_ERR_FILE_OPEN;

    /* Get file size */
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    long fsize = ftell(fp);
    if (fsize <= 0) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return UFT_ERR_IO;
    }

    size_t size = (size_t)fsize;
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(fp);
        return UFT_ERR_MEMORY;
    }

    if (fread(data, 1, size, fp) != size) {
        free(data);
        fclose(fp);
        return UFT_ERR_FILE_READ;
    }
    fclose(fp);

    int result = uft_fds_open_buffer(data, size, img);
    free(data);
    return result;
}

void uft_fds_close(uft_fds_image_t *img)
{
    if (!img) return;

    free(img->data);
    memset(img, 0, sizeof(uft_fds_image_t));
}

int uft_fds_get_info_string(const uft_fds_image_t *img, char *buf, size_t buflen)
{
    if (!img || !buf || buflen == 0) return UFT_ERR_NULL_POINTER;

    int written = snprintf(buf, buflen,
        "FDS Image:\n"
        "  Header:       %s\n"
        "  Sides:        %d\n"
        "  Game Name:    %.4s\n"
        "  Manufacturer: 0x%02X\n"
        "  Revision:     %d\n"
        "  Disk Number:  %d\n"
        "  Side Number:  %d\n"
        "  Files:        %d\n"
        "  Signature:    %s\n"
        "  Data Size:    %zu bytes\n",
        img->has_header ? "fwNES (FDS\\x1A)" : "headerless",
        img->side_count,
        img->game_name,
        img->manufacturer_code,
        img->revision,
        img->disk_number,
        img->side_number,
        img->file_count,
        img->valid_signature ? "*NINTENDO-HVC* OK" : "missing/invalid",
        img->data_size);

    if (written < 0) return UFT_ERR_IO;
    return 0;
}
