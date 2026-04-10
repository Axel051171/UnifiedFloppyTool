/**
 * @file uft_dart.c
 * @brief DART (Disk Archive/Retrieval Tool) format parser implementation
 *
 * DART archives Mac floppy disks with an 84-byte header followed by
 * either raw or RLE-compressed sector data.
 *
 * Header (84 bytes, big-endian):
 *   0-63:  Disk name (Pascal string: length byte + chars)
 *   64-67: Block count (number of 512-byte sectors)
 *   68-71: Checksum
 *   72:    Data type (0=uncompressed, 1=RLE)
 *   73:    Disk type (0=400K, 1=800K, 2=720K, 3=1440K)
 *   74-83: Reserved
 *
 * RLE encoding uses 0x90 as escape:
 *   0x90 0x00        -> literal 0x90
 *   0x90 <count>     -> repeat previous byte <count> times
 *   <byte>           -> literal byte
 *
 * @author UFT Project
 * @date 2026-04-10
 */

#include "uft/formats/apple/uft_dart.h"
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

/**
 * @brief Expected data size for a DART disk type
 */
static uint32_t dart_expected_size(dart_disk_type_t dt) {
    switch (dt) {
        case DART_DISK_400K:  return 409600u;
        case DART_DISK_800K:  return 819200u;
        case DART_DISK_720K:  return 737280u;
        case DART_DISK_1440K: return 1474560u;
        default:              return 0;
    }
}

/**
 * @brief Expected block count for a DART disk type
 */
static uint32_t dart_expected_blocks(dart_disk_type_t dt) {
    return dart_expected_size(dt) / DART_SECTOR_SIZE;
}

/**
 * @brief Check if filename has a DART extension (case-insensitive)
 */
static bool has_dart_extension(const char *filename) {
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

    return (strcmp(lower, "dart") == 0 ||
            strcmp(lower, "image") == 0);
}

/* ============================================================================
 * Detection
 * ============================================================================ */

bool uft_dart_detect(const uint8_t *data, size_t size, const char *filename) {
    if (!data || size < DART_HEADER_SIZE) return false;

    /*
     * DART has no fixed magic. Detection is heuristic:
     * 1. Valid Pascal string at offset 0 (length 1-63, printable chars)
     * 2. Valid disk type at byte 73 (0-3)
     * 3. Valid data type at byte 72 (0 or 1)
     * 4. Block count matches disk type
     * 5. Bonus: matching extension
     */

    /* Check Pascal string */
    uint8_t name_len = data[0];
    if (name_len == 0 || name_len > DART_MAX_NAME_LEN) return false;

    bool valid_name = true;
    for (int i = 0; i < name_len; i++) {
        uint8_t c = data[1 + i];
        /* Allow printable ASCII and high-ASCII Mac chars */
        if (c < 0x20 && c != 0) {
            valid_name = false;
            break;
        }
    }
    if (!valid_name) return false;

    /* Check data type */
    uint8_t data_type = data[72];
    if (data_type > 1) return false;

    /* Check disk type */
    uint8_t disk_type = data[73];
    if (disk_type > 3) return false;

    /* Check block count matches disk type */
    uint32_t block_count = read_be32(data + 64);
    uint32_t expected = dart_expected_blocks((dart_disk_type_t)disk_type);
    if (expected > 0 && block_count != expected) return false;

    /* If we have a matching extension, it adds confidence */
    if (has_dart_extension(filename)) return true;

    /*
     * Without extension, require more stringent checks:
     * - Reserved bytes should be zero
     */
    bool reserved_zero = true;
    for (int i = 74; i < 84; i++) {
        if (data[i] != 0) {
            reserved_zero = false;
            break;
        }
    }

    return reserved_zero;
}

/* ============================================================================
 * Parsing
 * ============================================================================ */

uft_dart_error_t uft_dart_parse(const uint8_t *data, size_t size,
                                  uft_dart_context_t *ctx) {
    if (!data || !ctx) return UFT_DART_ERR_NULL;
    if (size < DART_HEADER_SIZE) return UFT_DART_ERR_TRUNCATED;

    memset(ctx, 0, sizeof(*ctx));

    /* Parse header */
    uint8_t name_len = data[0];
    if (name_len == 0 || name_len > DART_MAX_NAME_LEN) {
        return UFT_DART_ERR_INVALID;
    }

    memcpy(ctx->volume_name, data + 1, name_len);
    ctx->volume_name[name_len] = '\0';

    ctx->block_count = read_be32(data + 64);
    ctx->stored_checksum = read_be32(data + 68);
    ctx->data_type = (dart_data_type_t)data[72];
    ctx->disk_type = (dart_disk_type_t)data[73];

    /* Validate data type */
    if (ctx->data_type > DART_DATA_RLE) {
        return UFT_DART_ERR_INVALID;
    }

    /* Validate disk type */
    if (ctx->disk_type > DART_DISK_1440K) {
        return UFT_DART_ERR_INVALID;
    }

    /* Calculate expected size */
    uint32_t expected_data = dart_expected_size(ctx->disk_type);
    if (expected_data == 0) {
        return UFT_DART_ERR_INVALID;
    }

    uint32_t expected_blocks = expected_data / DART_SECTOR_SIZE;
    if (ctx->block_count != expected_blocks) {
        return UFT_DART_ERR_INVALID;
    }

    ctx->data_size = expected_data;
    ctx->sector_count = expected_blocks;
    ctx->sector_size = DART_SECTOR_SIZE;
    ctx->data_offset = DART_HEADER_SIZE;

    /* For uncompressed images, verify file is large enough */
    if (ctx->data_type == DART_DATA_UNCOMPRESSED) {
        if (size < DART_HEADER_SIZE + expected_data) {
            return UFT_DART_ERR_TRUNCATED;
        }

        /* Calculate and verify checksum */
        ctx->calculated_checksum = dart_calculate_checksum(
            data + DART_HEADER_SIZE, expected_data);
        ctx->checksum_valid = (ctx->calculated_checksum == ctx->stored_checksum);
    } else {
        /* For RLE, we cannot verify checksum until after decompression */
        ctx->checksum_valid = false;
    }

    ctx->is_valid = true;

    snprintf(ctx->description, sizeof(ctx->description),
             "DART %s %s \"%s\"",
             uft_dart_disk_type_string(ctx->disk_type),
             ctx->data_type == DART_DATA_RLE ? "RLE" : "Raw",
             ctx->volume_name);

    return UFT_DART_OK;
}

/* ============================================================================
 * RLE Decompression
 * ============================================================================ */

int dart_rle_decompress(const uint8_t *input, size_t input_size,
                         uint8_t *output, size_t output_size) {
    if (!input || !output) return -1;

    size_t in_pos = 0;
    size_t out_pos = 0;
    uint8_t prev_byte = 0;

    while (in_pos < input_size && out_pos < output_size) {
        uint8_t b = input[in_pos++];

        if (b == DART_RLE_ESCAPE) {
            /* Escape sequence */
            if (in_pos >= input_size) {
                return -1; /* Truncated */
            }
            uint8_t count = input[in_pos++];

            if (count == 0) {
                /* Literal 0x90 */
                if (out_pos >= output_size) return -1;
                output[out_pos++] = DART_RLE_ESCAPE;
                prev_byte = DART_RLE_ESCAPE;
            } else {
                /* Repeat previous byte 'count' times */
                /* Note: the previous byte was already output once,
                 * so we repeat (count - 1) additional times for a
                 * total of 'count' occurrences (including the original).
                 * Some DART implementations repeat 'count' times total,
                 * we follow the convention where the byte before the
                 * escape was already output, so count is additional repeats. */
                if (out_pos + count - 1 > output_size) return -1;
                for (int i = 0; i < count - 1; i++) {
                    output[out_pos++] = prev_byte;
                }
            }
        } else {
            if (out_pos >= output_size) return -1;
            output[out_pos++] = b;
            prev_byte = b;
        }
    }

    return (int)out_pos;
}

/* ============================================================================
 * Extraction
 * ============================================================================ */

int uft_dart_extract(const uint8_t *data, size_t size,
                      const uft_dart_context_t *ctx,
                      uint8_t *output, size_t output_size) {
    if (!data || !ctx || !output) return UFT_DART_ERR_NULL;
    if (!ctx->is_valid) return UFT_DART_ERR_INVALID;
    if (output_size < ctx->data_size) return UFT_DART_ERR_SIZE;

    if (ctx->data_type == DART_DATA_UNCOMPRESSED) {
        /* Direct copy */
        if (size < ctx->data_offset + ctx->data_size) {
            return UFT_DART_ERR_TRUNCATED;
        }
        memcpy(output, data + ctx->data_offset, ctx->data_size);
        return (int)ctx->data_size;
    }

    /* RLE decompression */
    size_t compressed_size = size - ctx->data_offset;
    if (compressed_size == 0) {
        return UFT_DART_ERR_TRUNCATED;
    }

    int result = dart_rle_decompress(
        data + ctx->data_offset, compressed_size,
        output, output_size);

    if (result < 0) {
        return UFT_DART_ERR_DECOMPRESS;
    }

    /* Verify we got the expected amount of data */
    if ((uint32_t)result != ctx->data_size) {
        /* Allow some tolerance -- some DART files may have trailing padding */
        if ((uint32_t)result < ctx->data_size) {
            /* Zero-fill the rest */
            memset(output + result, 0, ctx->data_size - (uint32_t)result);
        }
    }

    return (int)ctx->data_size;
}

/* ============================================================================
 * Checksum
 * ============================================================================ */

uint32_t dart_calculate_checksum(const uint8_t *data, size_t size) {
    if (!data || size == 0) return 0;

    /*
     * DART checksum: simple additive checksum of all bytes,
     * accumulated as big-endian 32-bit words.
     * If size is not a multiple of 4, pad with zeros.
     */
    uint32_t checksum = 0;
    size_t i;

    for (i = 0; i + 3 < size; i += 4) {
        uint32_t word = ((uint32_t)data[i] << 24) |
                        ((uint32_t)data[i+1] << 16) |
                        ((uint32_t)data[i+2] << 8) |
                        data[i+3];
        checksum += word;
    }

    /* Handle remaining bytes */
    if (i < size) {
        uint32_t word = 0;
        size_t remaining = size - i;
        for (size_t j = 0; j < remaining; j++) {
            word |= (uint32_t)data[i + j] << (24 - j * 8);
        }
        checksum += word;
    }

    return checksum;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char *uft_dart_disk_type_string(dart_disk_type_t disk_type) {
    switch (disk_type) {
        case DART_DISK_400K:    return "400K Mac GCR";
        case DART_DISK_800K:    return "800K Mac GCR";
        case DART_DISK_720K:    return "720K MFM";
        case DART_DISK_1440K:   return "1440K MFM HD";
        default:                return "Unknown";
    }
}

const char *uft_dart_strerror(uft_dart_error_t err) {
    switch (err) {
        case UFT_DART_OK:               return "Success";
        case UFT_DART_ERR_NULL:         return "Null pointer";
        case UFT_DART_ERR_OPEN:         return "Cannot open file";
        case UFT_DART_ERR_READ:         return "Read error";
        case UFT_DART_ERR_TRUNCATED:    return "File truncated";
        case UFT_DART_ERR_INVALID:      return "Invalid DART format";
        case UFT_DART_ERR_MEMORY:       return "Out of memory";
        case UFT_DART_ERR_DECOMPRESS:   return "RLE decompression failed";
        case UFT_DART_ERR_SIZE:         return "Invalid or mismatched size";
        case UFT_DART_ERR_CHECKSUM:     return "Checksum mismatch";
        default:                        return "Unknown error";
    }
}
