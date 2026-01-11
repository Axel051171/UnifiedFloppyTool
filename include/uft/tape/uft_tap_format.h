/**
 * @file uft_tap_format.h
 * @brief TAP Tape Format Support (ZX Spectrum)
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * TAP is the simple tape format for ZX Spectrum.
 * It stores raw tape blocks without timing information.
 * Each block has a 2-byte length header followed by data.
 *
 * Block structure:
 * - 2 bytes: Block length (little endian)
 * - N bytes: Data (flag + payload + checksum)
 *
 * Data structure:
 * - 1 byte: Flag (0x00=header, 0xFF=data)
 * - N-2 bytes: Payload
 * - 1 byte: XOR checksum
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_TAP_FORMAT_H
#define UFT_TAP_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * TAP Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief TAP header block size (flag + 17 bytes + checksum) */
#define UFT_TAP_HEADER_BLOCK_SIZE   19

/** @brief TAP flag for header block */
#define UFT_TAP_FLAG_HEADER         0x00

/** @brief TAP flag for data block */
#define UFT_TAP_FLAG_DATA           0xFF

/** @brief Maximum reasonable block size */
#define UFT_TAP_MAX_BLOCK_SIZE      65535

/* Header types */
#define UFT_TAP_HDR_PROGRAM         0       /**< BASIC program */
#define UFT_TAP_HDR_NUM_ARRAY       1       /**< Number array */
#define UFT_TAP_HDR_CHAR_ARRAY      2       /**< Character array */
#define UFT_TAP_HDR_CODE            3       /**< CODE/Bytes */

/* ═══════════════════════════════════════════════════════════════════════════
 * TAP Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief TAP block header (not the Spectrum header)
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t length;                /**< Block length (little endian) */
} uft_tap_block_header_t;
#pragma pack(pop)

/**
 * @brief ZX Spectrum tape header (17 bytes payload)
 * Same as in TZX but without flag byte
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t type;                   /**< 0=Program, 1=Num, 2=Char, 3=Code */
    char filename[10];              /**< Filename (space-padded) */
    uint16_t data_len;              /**< Data block length */
    uint16_t param1;                /**< Autostart line / Start address */
    uint16_t param2;                /**< Variable area start / unused */
} uft_tap_spectrum_header_t;
#pragma pack(pop)

/**
 * @brief TAP block information
 */
typedef struct {
    uint16_t length;                /**< Total block length */
    uint8_t flag;                   /**< Flag byte */
    uint32_t offset;                /**< Offset in file */
    bool is_header;
    bool checksum_ok;
    
    /* For header blocks */
    uint8_t header_type;
    char filename[11];
    uint16_t data_len;
    uint16_t param1;
    uint16_t param2;
} uft_tap_block_info_t;

/**
 * @brief TAP file information
 */
typedef struct {
    uint32_t total_size;
    uint32_t block_count;
    uint32_t header_count;
    uint32_t data_count;
    uint32_t data_size;             /**< Total data bytes */
    bool all_checksums_ok;
} uft_tap_file_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_tap_spectrum_header_t) == 17, "Spectrum header must be 17 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get header type name
 */
static inline const char* uft_tap_header_type_name(uint8_t type) {
    switch (type) {
        case UFT_TAP_HDR_PROGRAM:    return "Program";
        case UFT_TAP_HDR_NUM_ARRAY:  return "Number Array";
        case UFT_TAP_HDR_CHAR_ARRAY: return "Character Array";
        case UFT_TAP_HDR_CODE:       return "Bytes";
        default: return "Unknown";
    }
}

/**
 * @brief Calculate XOR checksum
 */
static inline uint8_t uft_tap_calc_checksum(const uint8_t* data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum ^= data[i];
    }
    return sum;
}

/**
 * @brief Verify block checksum
 */
static inline bool uft_tap_verify_block(const uint8_t* data, uint16_t len) {
    if (!data || len < 2) return false;
    
    /* Checksum covers flag + payload, should equal last byte */
    uint8_t calc = uft_tap_calc_checksum(data, len - 1);
    return calc == data[len - 1];
}

/**
 * @brief Extract filename from header
 */
static inline void uft_tap_get_filename(const uft_tap_spectrum_header_t* hdr,
                                         char* out, size_t max_len) {
    if (!hdr || !out || max_len < 11) return;
    
    int i;
    for (i = 0; i < 10 && hdr->filename[i] != ' '; i++) {
        out[i] = hdr->filename[i];
    }
    out[i] = '\0';
}

/**
 * @brief Probe for TAP format
 * @return Confidence score (0-100)
 */
static inline int uft_tap_probe(const uint8_t* data, size_t size) {
    if (!data || size < 21) return 0;  /* Minimum: 2-byte len + 19-byte header block */
    
    int score = 0;
    
    /* Read first block length */
    uint16_t first_len = data[0] | (data[1] << 8);
    
    /* First block should be a header (19 bytes) */
    if (first_len == 19) {
        score += 30;
        
        /* Check flag byte */
        if (data[2] == UFT_TAP_FLAG_HEADER) {
            score += 20;
            
            /* Check header type */
            if (data[3] <= 3) {
                score += 15;
            }
            
            /* Verify checksum */
            if (uft_tap_verify_block(data + 2, 19)) {
                score += 20;
            }
            
            /* Check filename is printable */
            bool valid_name = true;
            for (int i = 0; i < 10; i++) {
                uint8_t c = data[4 + i];
                if (c < 0x20 || c > 0x7F) {
                    valid_name = false;
                    break;
                }
            }
            if (valid_name) score += 15;
        }
    }
    
    return (score > 100) ? 100 : score;
}

/**
 * @brief Parse TAP block at offset
 */
static inline bool uft_tap_parse_block(const uint8_t* data, size_t size,
                                        size_t offset, uft_tap_block_info_t* info) {
    if (!data || !info || offset + 2 >= size) return false;
    
    memset(info, 0, sizeof(*info));
    
    info->offset = offset;
    info->length = data[offset] | (data[offset + 1] << 8);
    
    if (info->length == 0 || offset + 2 + info->length > size) {
        return false;
    }
    
    const uint8_t* block = data + offset + 2;
    info->flag = block[0];
    info->is_header = (info->flag == UFT_TAP_FLAG_HEADER);
    info->checksum_ok = uft_tap_verify_block(block, info->length);
    
    /* Parse header content */
    if (info->is_header && info->length == 19) {
        const uft_tap_spectrum_header_t* hdr = 
            (const uft_tap_spectrum_header_t*)(block + 1);
        
        info->header_type = hdr->type;
        uft_tap_get_filename(hdr, info->filename, sizeof(info->filename));
        info->data_len = hdr->data_len;
        info->param1 = hdr->param1;
        info->param2 = hdr->param2;
    }
    
    return true;
}

/**
 * @brief Parse entire TAP file
 */
static inline bool uft_tap_parse_file(const uint8_t* data, size_t size,
                                       uft_tap_file_info_t* info) {
    if (!data || !info) return false;
    
    memset(info, 0, sizeof(*info));
    info->total_size = size;
    info->all_checksums_ok = true;
    
    size_t offset = 0;
    
    while (offset + 2 < size) {
        uint16_t len = data[offset] | (data[offset + 1] << 8);
        
        if (len == 0 || offset + 2 + len > size) break;
        
        info->block_count++;
        
        const uint8_t* block = data + offset + 2;
        
        if (block[0] == UFT_TAP_FLAG_HEADER) {
            info->header_count++;
        } else {
            info->data_count++;
            info->data_size += len - 2;  /* Exclude flag and checksum */
        }
        
        if (!uft_tap_verify_block(block, len)) {
            info->all_checksums_ok = false;
        }
        
        offset += 2 + len;
    }
    
    return info->block_count > 0;
}

/**
 * @brief Print TAP file info
 */
static inline void uft_tap_print_info(const uft_tap_file_info_t* info) {
    if (!info) return;
    
    printf("TAP File Information:\n");
    printf("  Size:       %u bytes\n", info->total_size);
    printf("  Blocks:     %u total\n", info->block_count);
    printf("  Headers:    %u\n", info->header_count);
    printf("  Data:       %u (%u bytes)\n", info->data_count, info->data_size);
    printf("  Checksums:  %s\n", info->all_checksums_ok ? "OK" : "ERRORS");
}

/**
 * @brief List all blocks in TAP file
 */
static inline void uft_tap_list_blocks(const uint8_t* data, size_t size) {
    if (!data) return;
    
    printf("TAP Blocks:\n");
    printf("  #  Flag  Length  Type        Filename    Checksum\n");
    printf("─────────────────────────────────────────────────────────\n");
    
    size_t offset = 0;
    int block_num = 0;
    
    while (offset + 2 < size) {
        uft_tap_block_info_t info;
        if (!uft_tap_parse_block(data, size, offset, &info)) break;
        
        if (info.is_header) {
            printf("%3d  0x%02X  %5u   %-10s  %-10s  %s\n",
                   block_num++, info.flag, info.length,
                   uft_tap_header_type_name(info.header_type),
                   info.filename,
                   info.checksum_ok ? "OK" : "BAD");
        } else {
            printf("%3d  0x%02X  %5u   DATA        -           %s\n",
                   block_num++, info.flag, info.length,
                   info.checksum_ok ? "OK" : "BAD");
        }
        
        offset += 2 + info.length;
    }
}

/**
 * @brief Create TAP header block
 */
static inline size_t uft_tap_create_header(uint8_t* out, size_t max_len,
                                            uint8_t type, const char* filename,
                                            uint16_t data_len, uint16_t param1,
                                            uint16_t param2) {
    if (!out || max_len < 21) return 0;
    
    /* Block length */
    out[0] = 19;
    out[1] = 0;
    
    /* Flag byte */
    out[2] = UFT_TAP_FLAG_HEADER;
    
    /* Header type */
    out[3] = type;
    
    /* Filename (space-padded) */
    memset(out + 4, ' ', 10);
    if (filename) {
        size_t name_len = strlen(filename);
        if (name_len > 10) name_len = 10;
        memcpy(out + 4, filename, name_len);
    }
    
    /* Data length */
    out[14] = data_len & 0xFF;
    out[15] = (data_len >> 8) & 0xFF;
    
    /* Param1 */
    out[16] = param1 & 0xFF;
    out[17] = (param1 >> 8) & 0xFF;
    
    /* Param2 */
    out[18] = param2 & 0xFF;
    out[19] = (param2 >> 8) & 0xFF;
    
    /* Checksum */
    out[20] = uft_tap_calc_checksum(out + 2, 18);
    
    return 21;
}

/**
 * @brief Create TAP data block
 */
static inline size_t uft_tap_create_data(uint8_t* out, size_t max_len,
                                          const uint8_t* data, uint16_t data_len) {
    if (!out || !data || max_len < data_len + 4) return 0;
    
    uint16_t block_len = data_len + 2;  /* flag + data + checksum */
    
    /* Block length */
    out[0] = block_len & 0xFF;
    out[1] = (block_len >> 8) & 0xFF;
    
    /* Flag byte */
    out[2] = UFT_TAP_FLAG_DATA;
    
    /* Data */
    memcpy(out + 3, data, data_len);
    
    /* Checksum */
    out[3 + data_len] = uft_tap_calc_checksum(out + 2, data_len + 1);
    
    return 4 + data_len;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_TAP_FORMAT_H */
