/**
 * @file uft_ndif.c
 * @brief NDIF (New Disk Image Format / DiskCopy 6.x) parser implementation
 *
 * Supports:
 * - Flat (data-fork-only) NDIF images: raw sector data
 * - Resource-fork NDIF images: BLKX block map with raw/ADC/zero blocks
 *
 * Detection strategy:
 * - Extension .img/.dmg with floppy-sized file -> flat NDIF
 * - Resource fork signature "bcem" -> resource-fork NDIF
 * - Size heuristic for standard Mac floppy sizes
 *
 * @author UFT Project
 * @date 2026-04-10
 */

#include "uft/formats/apple/uft_ndif.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static inline uint32_t read_be32(const uint8_t *p) {
    return (uint32_t)(((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                      ((uint32_t)p[2] << 8)  | p[3]);
}

static inline uint64_t read_be64(const uint8_t *p) {
    return ((uint64_t)read_be32(p) << 32) | read_be32(p + 4);
}

/**
 * @brief Determine disk type from data size
 */
static ndif_disk_type_t ndif_disk_type_from_size(uint32_t size) {
    switch (size) {
        case NDIF_SIZE_400K:  return NDIF_DISK_400K;
        case NDIF_SIZE_800K:  return NDIF_DISK_800K;
        case NDIF_SIZE_720K:  return NDIF_DISK_720K;
        case NDIF_SIZE_1440K: return NDIF_DISK_1440K;
        default:              return NDIF_DISK_UNKNOWN;
    }
}

/**
 * @brief Check if filename has a relevant extension
 */
static bool has_ndif_extension(const char *filename) {
    if (!filename) return false;

    const char *dot = strrchr(filename, '.');
    if (!dot || !dot[1]) return false;

    const char *ext = dot + 1;
    size_t len = strlen(ext);
    if (len > 8) return false;

    char lower[16];
    for (size_t i = 0; i < len && i < sizeof(lower) - 1; i++) {
        char c = ext[i];
        lower[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    }
    lower[len < sizeof(lower) - 1 ? len : sizeof(lower) - 1] = '\0';

    return (strcmp(lower, "img") == 0 ||
            strcmp(lower, "dmg") == 0 ||
            strcmp(lower, "image") == 0 ||
            strcmp(lower, "smi") == 0);
}

/**
 * @brief Decompress ADC (Apple Data Compression) block
 *
 * ADC is a simple LZ77-style scheme:
 *   - If high bit clear (0x00-0x7F): literal run, count = (byte & 0x7F) + 1
 *   - If 0x80-0xBF: 2-byte match, distance/length encoded
 *   - If 0xC0-0xFF: 3-byte match for longer distances
 */
static int adc_decompress_block(const uint8_t *input, size_t input_size,
                                 uint8_t *output, size_t output_size) {
    size_t in_pos = 0;
    size_t out_pos = 0;

    while (in_pos < input_size && out_pos < output_size) {
        uint8_t cmd = input[in_pos++];

        if ((cmd & 0x80) == 0) {
            /* Literal run: copy (cmd + 1) bytes */
            size_t count = (size_t)(cmd & 0x7F) + 1;
            if (in_pos + count > input_size) return -1;
            if (out_pos + count > output_size) return -1;
            memcpy(output + out_pos, input + in_pos, count);
            in_pos += count;
            out_pos += count;
        } else if ((cmd & 0xC0) == 0x80) {
            /* 2-byte match: length = (cmd & 0x3F) + 3, offset from next byte */
            if (in_pos >= input_size) return -1;
            size_t length = (size_t)(cmd & 0x3F) + 3;
            size_t offset = (size_t)input[in_pos++] + 1;
            if (offset > out_pos) return -1;
            if (out_pos + length > output_size) return -1;
            size_t src = out_pos - offset;
            for (size_t i = 0; i < length; i++) {
                output[out_pos++] = output[src + (i % offset)];
            }
        } else {
            /* 3-byte match: length = (cmd & 0x3F) + 4, 16-bit offset */
            if (in_pos + 1 >= input_size) return -1;
            size_t length = (size_t)(cmd & 0x3F) + 4;
            size_t offset = ((size_t)input[in_pos] << 8) | input[in_pos + 1];
            in_pos += 2;
            offset += 1;
            if (offset > out_pos) return -1;
            if (out_pos + length > output_size) return -1;
            size_t src = out_pos - offset;
            for (size_t i = 0; i < length; i++) {
                output[out_pos++] = output[src + (i % offset)];
            }
        }
    }

    return (int)out_pos;
}

/* ============================================================================
 * Detection
 * ============================================================================ */

bool uft_ndif_detect(const uint8_t *data, size_t size, const char *filename) {
    if (!data || size < 16) return false;

    /* Check for resource-fork NDIF: look for "bcem" signature */
    /* This can appear at various offsets; check common locations */
    if (size >= 260) {
        /* Scan first 256 bytes for "bcem" */
        for (size_t off = 0; off + 4 <= 256 && off + 4 <= size; off++) {
            if (data[off] == 'b' && data[off+1] == 'c' &&
                data[off+2] == 'e' && data[off+3] == 'm') {
                return true;
            }
        }
    }

    /* Flat (data-fork-only) detection: correct floppy size + extension */
    if (has_ndif_extension(filename)) {
        if (size == NDIF_SIZE_400K || size == NDIF_SIZE_800K ||
            size == NDIF_SIZE_720K || size == NDIF_SIZE_1440K) {
            return true;
        }
    }

    return false;
}

/* ============================================================================
 * Parsing
 * ============================================================================ */

/**
 * @brief Try to parse as a flat (data-fork-only) NDIF image
 */
static uft_ndif_error_t parse_flat(const uint8_t *data, size_t size,
                                    uft_ndif_context_t *ctx) {
    ndif_disk_type_t dt = ndif_disk_type_from_size((uint32_t)size);
    if (dt == NDIF_DISK_UNKNOWN) {
        return UFT_NDIF_ERR_SIZE;
    }

    ctx->subtype = NDIF_SUBTYPE_FLAT;
    ctx->disk_type = dt;
    ctx->data_size = (uint32_t)size;
    ctx->sector_count = (uint32_t)(size / NDIF_SECTOR_SIZE);
    ctx->sector_size = NDIF_SECTOR_SIZE;
    ctx->block_count = 0;
    ctx->is_valid = true;

    snprintf(ctx->description, sizeof(ctx->description),
             "NDIF Flat %s", uft_ndif_disk_type_string(dt));

    return UFT_NDIF_OK;
}

/**
 * @brief Try to parse as a resource-fork NDIF image with BLKX entries
 *
 * Resource-fork NDIF layout:
 *   - Resource map at the end of the file or in a separate resource fork
 *   - BLKX ('bcem') resources contain block descriptors
 *   - Each BLKX entry is 40 bytes describing a data chunk
 *
 * For simplicity we scan for the "bcem" marker and parse BLKX entries
 * found after it.
 */
static uft_ndif_error_t parse_rsrc(const uint8_t *data, size_t size,
                                    uft_ndif_context_t *ctx) {
    /* Find "bcem" signature */
    size_t bcem_off = 0;
    bool found = false;
    for (size_t off = 0; off + 4 <= size; off++) {
        if (data[off] == 'b' && data[off+1] == 'c' &&
            data[off+2] == 'e' && data[off+3] == 'm') {
            bcem_off = off;
            found = true;
            break;
        }
    }

    if (!found) {
        return UFT_NDIF_ERR_INVALID;
    }

    ctx->subtype = NDIF_SUBTYPE_RSRC;
    ctx->block_count = 0;

    /*
     * After "bcem", there is typically a block descriptor table.
     * The exact layout varies, but we look for a 4-byte count
     * followed by 40-byte BLKX entries.
     */
    size_t pos = bcem_off + 4;

    /* Skip potential padding/version bytes (up to 200 bytes) to find entries */
    /* We look for the pattern where sector_count and compressed_length are
     * plausible (non-zero, within file bounds). */
    uint64_t total_sectors = 0;

    /* Try to parse BLKX entries starting from various offsets after "bcem" */
    for (size_t skip = 0; skip < 200 && pos + skip + 40 <= size; skip += 4) {
        size_t try_pos = pos + skip;
        /* Check if this looks like a valid BLKX entry */
        uint32_t btype = read_be32(data + try_pos);

        /* Valid block types */
        if (btype == NDIF_BLK_RAW || btype == NDIF_BLK_ZERO ||
            btype == NDIF_BLK_ADC || btype == (uint32_t)NDIF_BLK_ZLIB) {
            /* Parse entries from here */
            size_t entry_pos = try_pos;
            while (entry_pos + 40 <= size && ctx->block_count < NDIF_MAX_BLOCKS) {
                uint32_t type = read_be32(data + entry_pos);

                if (type == (uint32_t)NDIF_BLK_TERM || type == (uint32_t)NDIF_BLK_COMMENT) {
                    break;
                }

                if (type != NDIF_BLK_RAW && type != NDIF_BLK_ZERO &&
                    type != NDIF_BLK_ADC && type != (uint32_t)NDIF_BLK_ZLIB) {
                    break;
                }

                ndif_blkx_entry_t *blk = &ctx->blocks[ctx->block_count];
                blk->block_type = type;
                blk->reserved = read_be32(data + entry_pos + 4);
                blk->sector_offset = read_be64(data + entry_pos + 8);
                blk->sector_count = read_be64(data + entry_pos + 16);
                blk->compressed_offset = read_be64(data + entry_pos + 24);
                blk->compressed_length = read_be64(data + entry_pos + 32);

                total_sectors += blk->sector_count;
                ctx->block_count++;
                entry_pos += 40;
            }
            break;
        }
    }

    if (ctx->block_count == 0) {
        return UFT_NDIF_ERR_INVALID;
    }

    ctx->sector_count = (uint32_t)total_sectors;
    ctx->sector_size = NDIF_SECTOR_SIZE;
    ctx->data_size = ctx->sector_count * NDIF_SECTOR_SIZE;
    ctx->disk_type = ndif_disk_type_from_size(ctx->data_size);
    ctx->is_valid = true;

    snprintf(ctx->description, sizeof(ctx->description),
             "NDIF Resource Fork %s (%d blocks)",
             uft_ndif_disk_type_string(ctx->disk_type), ctx->block_count);

    return UFT_NDIF_OK;
}

uft_ndif_error_t uft_ndif_parse(const uint8_t *data, size_t size,
                                 uft_ndif_context_t *ctx) {
    if (!data || !ctx) return UFT_NDIF_ERR_NULL;
    if (size < 16) return UFT_NDIF_ERR_TRUNCATED;

    memset(ctx, 0, sizeof(*ctx));

    /* Try resource-fork format first (has "bcem" marker) */
    for (size_t off = 0; off + 4 <= size && off < 256; off++) {
        if (data[off] == 'b' && data[off+1] == 'c' &&
            data[off+2] == 'e' && data[off+3] == 'm') {
            return parse_rsrc(data, size, ctx);
        }
    }

    /* Fall back to flat format */
    return parse_flat(data, size, ctx);
}

/* ============================================================================
 * Extraction
 * ============================================================================ */

int uft_ndif_extract(const uint8_t *data, size_t size,
                      const uft_ndif_context_t *ctx,
                      uint8_t *output, size_t output_size) {
    if (!data || !ctx || !output) return UFT_NDIF_ERR_NULL;
    if (!ctx->is_valid) return UFT_NDIF_ERR_INVALID;

    if (ctx->subtype == NDIF_SUBTYPE_FLAT) {
        /* Flat image: just copy the raw data */
        uint32_t copy_size = ctx->data_size;
        if (copy_size > size) return UFT_NDIF_ERR_TRUNCATED;
        if (copy_size > output_size) return UFT_NDIF_ERR_SIZE;

        memcpy(output, data, copy_size);
        return (int)copy_size;
    }

    /* Resource-fork: process BLKX entries */
    if (output_size < ctx->data_size) return UFT_NDIF_ERR_SIZE;

    /* Zero the output first (handles zero-fill blocks implicitly) */
    memset(output, 0, ctx->data_size);

    for (int i = 0; i < ctx->block_count; i++) {
        const ndif_blkx_entry_t *blk = &ctx->blocks[i];
        uint64_t out_offset = blk->sector_offset * NDIF_SECTOR_SIZE;
        uint64_t out_length = blk->sector_count * NDIF_SECTOR_SIZE;

        if (out_offset + out_length > output_size) {
            return UFT_NDIF_ERR_SIZE;
        }

        switch (blk->block_type) {
            case NDIF_BLK_ZERO:
                /* Already zeroed */
                break;

            case NDIF_BLK_RAW:
                if (blk->compressed_offset + blk->compressed_length > size) {
                    return UFT_NDIF_ERR_TRUNCATED;
                }
                memcpy(output + out_offset,
                       data + blk->compressed_offset,
                       (size_t)blk->compressed_length);
                break;

            case NDIF_BLK_ADC: {
                if (blk->compressed_offset + blk->compressed_length > size) {
                    return UFT_NDIF_ERR_TRUNCATED;
                }
                int result = adc_decompress_block(
                    data + blk->compressed_offset,
                    (size_t)blk->compressed_length,
                    output + out_offset,
                    (size_t)out_length);
                if (result < 0) {
                    return UFT_NDIF_ERR_DECOMPRESS;
                }
                break;
            }

            default:
                /* Unsupported compression (zlib etc.) -- skip */
                return UFT_NDIF_ERR_UNSUPPORTED;
        }
    }

    return (int)ctx->data_size;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char *uft_ndif_disk_type_string(ndif_disk_type_t disk_type) {
    switch (disk_type) {
        case NDIF_DISK_400K:    return "400K Mac GCR";
        case NDIF_DISK_800K:    return "800K Mac GCR";
        case NDIF_DISK_720K:    return "720K MFM";
        case NDIF_DISK_1440K:   return "1440K MFM HD";
        default:                return "Unknown";
    }
}

const char *uft_ndif_strerror(uft_ndif_error_t err) {
    switch (err) {
        case UFT_NDIF_OK:               return "Success";
        case UFT_NDIF_ERR_NULL:         return "Null pointer";
        case UFT_NDIF_ERR_OPEN:         return "Cannot open file";
        case UFT_NDIF_ERR_READ:         return "Read error";
        case UFT_NDIF_ERR_TRUNCATED:    return "File truncated";
        case UFT_NDIF_ERR_INVALID:      return "Invalid NDIF format";
        case UFT_NDIF_ERR_MEMORY:       return "Out of memory";
        case UFT_NDIF_ERR_UNSUPPORTED:  return "Unsupported compression";
        case UFT_NDIF_ERR_DECOMPRESS:   return "Decompression failed";
        case UFT_NDIF_ERR_SIZE:         return "Invalid or mismatched size";
        default:                        return "Unknown error";
    }
}
