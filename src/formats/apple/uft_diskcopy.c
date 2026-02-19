/**
 * @file uft_diskcopy.c
 * @brief Apple Disk Copy and NDIF disk image format implementation
 * 
 * Supports Disk Copy 4.2, NDIF (Disk Copy 6.x), SMI, and MacBinary II
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#include "uft/formats/apple/uft_diskcopy.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Read big-endian 16-bit value
 */
static inline uint16_t read_be16(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

/**
 * @brief Read big-endian 32-bit value
 */
static inline uint32_t read_be32(const uint8_t *p) {
    return (uint32_t)(((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | (p[2] << 8) | p[3]);
}

/**
 * @brief Write big-endian 16-bit value
 */
static inline void write_be16(uint8_t *p, uint16_t v) {
    p[0] = (v >> 8) & 0xFF;
    p[1] = v & 0xFF;
}

/**
 * @brief Write big-endian 32-bit value
 */
static inline void write_be32(uint8_t *p, uint32_t v) {
    p[0] = (v >> 24) & 0xFF;
    p[1] = (v >> 16) & 0xFF;
    p[2] = (v >> 8) & 0xFF;
    p[3] = v & 0xFF;
}

/**
 * @brief Calculate MacBinary CRC-16
 * CRC-CCITT polynomial 0x1021
 */
static uint16_t macbinary_crc(const uint8_t *data, size_t size) {
    uint16_t crc = 0;
    
    for (size_t i = 0; i < size; i++) {
        uint8_t byte = data[i];
        for (int bit = 0; bit < 8; bit++) {
            bool xor_flag = (crc & 0x8000) != 0;
            crc <<= 1;
            if (byte & 0x80) {
                crc |= 1;
            }
            if (xor_flag) {
                crc ^= 0x1021;
            }
            byte <<= 1;
        }
    }
    
    /* Final 16 zero bits */
    for (int i = 0; i < 16; i++) {
        bool xor_flag = (crc & 0x8000) != 0;
        crc <<= 1;
        if (xor_flag) {
            crc ^= 0x1021;
        }
    }
    
    return crc;
}

/* ============================================================================
 * Format Detection
 * ============================================================================ */

macbinary_type_t dc_detect_macbinary(const uint8_t *data, size_t size) {
    if (!data || size < MACBINARY_HEADER_SIZE) {
        return MB_TYPE_NONE;
    }
    
    const macbinary_header_t *mb = (const macbinary_header_t *)data;
    
    /* Check required zero bytes */
    if (mb->old_version != 0 || mb->zero1 != 0 || mb->zero2 != 0) {
        return MB_TYPE_NONE;
    }
    
    /* Check filename length (1-63) */
    if (mb->filename_len == 0 || mb->filename_len > 63) {
        return MB_TYPE_NONE;
    }
    
    /* Check filename contains printable ASCII */
    for (int i = 0; i < mb->filename_len; i++) {
        if (mb->filename[i] < 0x20 || mb->filename[i] > 0x7E) {
            /* Allow high-ASCII Mac characters */
            if (mb->filename[i] < 0x80) {
                return MB_TYPE_NONE;
            }
        }
    }
    
    /* Get fork lengths */
    uint32_t data_len = read_be32((const uint8_t *)&mb->data_fork_len);
    uint32_t rsrc_len = read_be32((const uint8_t *)&mb->rsrc_fork_len);
    
    /* Sanity check sizes */
    uint32_t expected_size = MACBINARY_HEADER_SIZE + 
                             ((data_len + 127) & ~127) +
                             ((rsrc_len + 127) & ~127);
    
    if (expected_size > size + 256) {  /* Allow some slack */
        return MB_TYPE_NONE;
    }
    
    /* Check version byte for MacBinary II/III */
    if (mb->version == 129 || mb->version == 130) {
        /* Verify CRC for MacBinary II/III */
        uint16_t stored_crc = read_be16((const uint8_t *)&mb->crc);
        uint16_t calc_crc = macbinary_crc(data, 124);
        
        if (stored_crc == calc_crc) {
            return (mb->version == 130) ? MB_TYPE_MACBINARY_III : MB_TYPE_MACBINARY_II;
        }
    }
    
    /* Could be MacBinary I (no CRC) */
    /* Additional heuristics: check file type is 4 printable chars */
    bool valid_type = true;
    for (int i = 0; i < 4; i++) {
        if (mb->file_type[i] < 0x20 || mb->file_type[i] > 0x7E) {
            if (mb->file_type[i] != 0) {
                valid_type = false;
                break;
            }
        }
    }
    
    if (valid_type && data_len > 0) {
        return MB_TYPE_MACBINARY_I;
    }
    
    return MB_TYPE_NONE;
}

dc_image_type_t dc_detect_format(const uint8_t *data, size_t size) {
    if (!data || size < 128) {
        return DC_TYPE_UNKNOWN;
    }
    
    /* Check for MacBinary wrapper first */
    macbinary_type_t mb_type = dc_detect_macbinary(data, size);
    uint32_t offset = 0;
    
    if (mb_type != MB_TYPE_NONE) {
        offset = MACBINARY_HEADER_SIZE;
    }
    
    if (offset + DC42_HEADER_SIZE > size) {
        return DC_TYPE_UNKNOWN;
    }
    
    const uint8_t *hdr = data + offset;
    
    /* Check for UDIF signature 'koly' at end of file */
    if (size >= 512 && memcmp(data + size - 512, "koly", 4) == 0) {
        return DC_TYPE_UDIF;
    }
    
    /* Check for Disk Copy 4.2 format */
    /* DC42 has: volume name (Pascal string), sizes, checksums, magic 0x0100 */
    uint8_t name_len = hdr[0];
    
    if (name_len > 0 && name_len <= 63) {
        /* Check if it looks like a valid Pascal string */
        bool valid_name = true;
        for (int i = 0; i < name_len && i < 63; i++) {
            uint8_t c = hdr[1 + i];
            if (c < 0x20 && c != 0) {
                valid_name = false;
                break;
            }
        }
        
        if (valid_name) {
            /* Check magic word at offset 82-83 */
            uint16_t magic = read_be16(hdr + 82);
            if (magic == 0x0100) {
                /* Verify sizes are reasonable */
                uint32_t data_size = read_be32(hdr + 64);
                uint32_t tag_size = read_be32(hdr + 68);
                
                if (data_size > 0 && data_size <= 0x1000000 &&  /* Max 16MB */
                    tag_size <= data_size) {
                    /* Check if we have enough data */
                    if (offset + DC42_HEADER_SIZE + data_size + tag_size <= size + 1024) {
                        return DC_TYPE_DC42;
                    }
                }
            }
        }
    }
    
    /* Check for SMI (Self-Mounting Image) */
    /* SMI has executable code followed by disk image */
    /* Look for 68K code patterns or 'CODE' resource indicator */
    if (mb_type != MB_TYPE_NONE) {
        /* Get file type */
        const macbinary_header_t *mb = (const macbinary_header_t *)data;
        if (memcmp(mb->file_type, "APPL", 4) == 0) {
            /* Application - likely SMI */
            /* Check for 'oneb' creator (Disk Copy SMI) */
            if (memcmp(mb->creator, "oneb", 4) == 0 ||
                memcmp(mb->creator, "dCpy", 4) == 0) {
                return DC_TYPE_SMI;
            }
        }
    }
    
    /* Check for NDIF without MacBinary */
    /* NDIF has 'bcem' resource in resource fork */
    /* For raw files, look for characteristic header */
    
    /* Check for raw disk image by size */
    if (size == DC_SIZE_400K || size == DC_SIZE_800K ||
        size == DC_SIZE_720K || size == DC_SIZE_1440K) {
        return DC_TYPE_RAW;
    }
    
    /* Could still be NDIF if it has the right structure */
    /* Check for typical NDIF header pattern */
    if (size > 0x500) {
        /* NDIF often has 'BD' (HFS signature) visible in header area */
        /* when the embedded disk is HFS */
        if (hdr[0] == 'B' && hdr[1] == 'D') {
            /* Might be NDIF or have HFS header visible */
            return DC_TYPE_NDIF;
        }
    }
    
    return DC_TYPE_UNKNOWN;
}

/* ============================================================================
 * Disk Copy 4.2 Parsing
 * ============================================================================ */

bool dc42_validate_header(const dc42_header_t *header) {
    if (!header) return false;
    
    /* Check magic word */
    uint16_t magic = read_be16((const uint8_t *)&header->private_word);
    if (magic != 0x0100) {
        return false;
    }
    
    /* Check volume name length */
    uint8_t name_len = header->volume_name[0];
    if (name_len == 0 || name_len > 63) {
        return false;
    }
    
    /* Check data size is reasonable */
    uint32_t data_size = read_be32((const uint8_t *)&header->data_size);
    if (data_size == 0 || data_size > 0x1000000) {
        return false;
    }
    
    /* Check disk encoding is valid */
    if (header->disk_encoding > DC_FORMAT_MFM_1440K &&
        header->disk_encoding != DC_FORMAT_CUSTOM) {
        return false;
    }
    
    return true;
}

int dc42_parse_header(const uint8_t *data, size_t size, dc_analysis_result_t *result) {
    if (!data || !result || size < DC42_HEADER_SIZE) {
        return -1;
    }
    
    const dc42_header_t *header = (const dc42_header_t *)data;
    
    if (!dc42_validate_header(header)) {
        return -2;
    }
    
    /* Extract volume name */
    uint8_t name_len = header->volume_name[0];
    memcpy(result->volume_name, header->volume_name + 1, name_len);
    result->volume_name[name_len] = '\0';
    
    /* Extract sizes */
    result->data_size = read_be32((const uint8_t *)&header->data_size);
    result->tag_size = read_be32((const uint8_t *)&header->tag_size);
    result->total_size = result->data_size + result->tag_size;
    
    /* Extract checksums */
    result->data_checksum = read_be32((const uint8_t *)&header->data_checksum);
    result->tag_checksum = read_be32((const uint8_t *)&header->tag_checksum);
    
    /* Disk format */
    result->disk_format = (dc_disk_format_t)header->disk_encoding;
    result->format_byte = header->format_byte;
    
    /* Calculate sectors */
    result->sector_size = 512;
    result->sector_count = result->data_size / result->sector_size;
    
    /* Set type */
    result->image_type = DC_TYPE_DC42;
    result->is_valid = true;
    
    /* Set offsets */
    result->header_offset = 0;  /* Will be adjusted by caller if MacBinary */
    result->data_offset = DC42_HEADER_SIZE;
    result->tag_offset = DC42_HEADER_SIZE + result->data_size;
    
    /* Format description */
    snprintf(result->format_description, sizeof(result->format_description),
             "Disk Copy 4.2 %s", dc_format_description(result->disk_format));
    
    return 0;
}

/* ============================================================================
 * MacBinary Parsing
 * ============================================================================ */

int macbinary_parse_header(const uint8_t *data, size_t size, dc_analysis_result_t *result) {
    if (!data || !result || size < MACBINARY_HEADER_SIZE) {
        return -1;
    }
    
    macbinary_type_t mb_type = dc_detect_macbinary(data, size);
    if (mb_type == MB_TYPE_NONE) {
        return -2;
    }
    
    const macbinary_header_t *mb = (const macbinary_header_t *)data;
    
    result->macbinary_type = mb_type;
    
    /* Extract filename */
    uint8_t name_len = mb->filename_len;
    memcpy(result->mb_filename, mb->filename, name_len);
    result->mb_filename[name_len] = '\0';
    
    /* Extract type and creator */
    memcpy(result->mb_type, mb->file_type, 4);
    result->mb_type[4] = '\0';
    memcpy(result->mb_creator, mb->creator, 4);
    result->mb_creator[4] = '\0';
    
    /* Extract fork lengths */
    result->mb_data_fork_len = read_be32((const uint8_t *)&mb->data_fork_len);
    result->mb_rsrc_fork_len = read_be32((const uint8_t *)&mb->rsrc_fork_len);
    
    /* Calculate offsets */
    result->data_offset = MACBINARY_HEADER_SIZE;
    result->rsrc_offset = MACBINARY_HEADER_SIZE + 
                          ((result->mb_data_fork_len + 127) & ~127);
    
    return 0;
}

/* ============================================================================
 * Main Analysis Function
 * ============================================================================ */

int dc_analyze(const uint8_t *data, size_t size, dc_analysis_result_t *result) {
    if (!data || !result || size < 128) {
        return -1;
    }
    
    memset(result, 0, sizeof(dc_analysis_result_t));
    
    /* Check for MacBinary wrapper */
    macbinary_type_t mb_type = dc_detect_macbinary(data, size);
    uint32_t content_offset = 0;
    
    if (mb_type != MB_TYPE_NONE) {
        macbinary_parse_header(data, size, result);
        content_offset = MACBINARY_HEADER_SIZE;
        result->header_offset = content_offset;
    }
    
    /* Detect image type */
    dc_image_type_t img_type = dc_detect_format(data, size);
    result->image_type = img_type;
    
    switch (img_type) {
        case DC_TYPE_DC42: {
            int ret = dc42_parse_header(data + content_offset, 
                                        size - content_offset, result);
            if (ret < 0) {
                return ret;
            }
            
            /* Adjust offsets for MacBinary */
            result->header_offset = content_offset;
            result->data_offset += content_offset;
            result->tag_offset += content_offset;
            
            /* Verify checksum */
            if (result->data_offset + result->data_size <= size) {
                result->calculated_checksum = dc_calculate_checksum(
                    data + result->data_offset, result->data_size);
                result->checksum_valid = (result->calculated_checksum == 
                                          result->data_checksum);
            }
            break;
        }
        
        case DC_TYPE_SMI: {
            /* SMI analysis */
            result->has_stub = true;
            result->stub_size = smi_detect_stub(data + content_offset,
                                                 size - content_offset);
            
            /* Try to find embedded disk image */
            /* The disk image header is typically after the stub */
            uint32_t img_offset = content_offset + result->stub_size;
            
            if (img_offset + DC42_HEADER_SIZE < size) {
                /* Check if there's a DC42 header after stub */
                dc_analysis_result_t inner;
                if (dc42_parse_header(data + img_offset, 
                                      size - img_offset, &inner) == 0) {
                    /* Copy relevant fields */
                    strncpy(result->volume_name, inner.volume_name, sizeof(result->volume_name) - 1);
                    result->volume_name[sizeof(result->volume_name) - 1] = '\0';
                    result->data_size = inner.data_size;
                    result->tag_size = inner.tag_size;
                    result->disk_format = inner.disk_format;
                    result->sector_count = inner.sector_count;
                    result->data_offset = img_offset + DC42_HEADER_SIZE;
                    result->is_valid = true;
                }
            }
            
            snprintf(result->format_description, sizeof(result->format_description),
                     "Self-Mounting Image (SMI)");
            break;
        }
        
        case DC_TYPE_NDIF: {
            /* NDIF analysis - resource fork based */
            result->is_compressed = ndif_is_compressed(data, size, result);
            
            snprintf(result->format_description, sizeof(result->format_description),
                     "NDIF (Disk Copy 6.x) %s",
                     result->is_compressed ? "[compressed]" : "[raw]");
            break;
        }
        
        case DC_TYPE_RAW: {
            /* Raw disk image */
            result->data_size = size;
            result->tag_size = 0;
            result->total_size = size;
            result->sector_size = 512;
            result->sector_count = size / 512;
            result->data_offset = 0;
            result->disk_format = dc_format_from_size(size);
            result->is_valid = true;
            
            snprintf(result->format_description, sizeof(result->format_description),
                     "Raw disk image %s", dc_format_description(result->disk_format));
            break;
        }
        
        case DC_TYPE_UDIF: {
            snprintf(result->format_description, sizeof(result->format_description),
                     "UDIF (.dmg) - not fully supported");
            break;
        }
        
        default:
            snprintf(result->format_description, sizeof(result->format_description),
                     "Unknown format");
            break;
    }
    
    return 0;
}

/* ============================================================================
 * Checksum Functions
 * ============================================================================ */

uint32_t dc_calculate_checksum(const uint8_t *data, size_t size) {
    if (!data || size == 0) return 0;
    
    uint32_t checksum = 0;
    
    /* Process in 16-bit words (big-endian) */
    for (size_t i = 0; i + 1 < size; i += 2) {
        uint16_t word = (data[i] << 8) | data[i + 1];
        
        /* Add word to checksum */
        checksum += word;
        
        /* Rotate right by 1 bit */
        checksum = (checksum >> 1) | ((checksum & 1) << 31);
    }
    
    /* Handle odd byte at end */
    if (size & 1) {
        checksum += data[size - 1] << 8;
        checksum = (checksum >> 1) | ((checksum & 1) << 31);
    }
    
    return checksum;
}

bool dc_verify_checksum(const uint8_t *data, size_t size,
                         const dc_analysis_result_t *result) {
    if (!data || !result) return false;
    
    if (result->data_offset + result->data_size > size) {
        return false;
    }
    
    uint32_t calc = dc_calculate_checksum(data + result->data_offset,
                                           result->data_size);
    
    return calc == result->data_checksum;
}

/* ============================================================================
 * Data Extraction
 * ============================================================================ */

int dc_extract_disk_data(const uint8_t *data, size_t size,
                          const dc_analysis_result_t *result,
                          uint8_t *output, size_t output_size) {
    if (!data || !result || !output) return -1;
    
    if (result->data_offset + result->data_size > size) {
        return -2;
    }
    
    if (output_size < result->data_size) {
        return -3;
    }
    
    /* For compressed NDIF, implement ADC (Apple Data Compression) decompression.
     * ADC is a simple run-length + LZ style compression used in NDIF images.
     * Format: Control byte determines the operation:
     *   0x00-0x7F: Copy (control+1) literal bytes from input
     *   0x80-0xBF: Repeat next byte (control-0x80+3) times  
     *   0xC0-0xFF: Copy (control-0xC0+4) bytes from offset in output */
    if (result->is_compressed) {
        const uint8_t *src = data + result->data_offset;
        size_t src_len = result->data_size;
        size_t src_pos = 0, dst_pos = 0;
        
        while (src_pos < src_len && dst_pos < output_size) {
            uint8_t ctrl = src[src_pos++];
            
            if (ctrl < 0x80) {
                /* Literal copy: ctrl+1 bytes */
                size_t count = (size_t)ctrl + 1;
                if (src_pos + count > src_len || dst_pos + count > output_size) break;
                memcpy(output + dst_pos, src + src_pos, count);
                src_pos += count;
                dst_pos += count;
            } else if (ctrl < 0xC0) {
                /* Byte run: repeat next byte (ctrl-0x80+3) times */
                size_t count = (size_t)(ctrl - 0x80) + 3;
                if (src_pos >= src_len || dst_pos + count > output_size) break;
                uint8_t val = src[src_pos++];
                memset(output + dst_pos, val, count);
                dst_pos += count;
            } else {
                /* Back-reference: copy (ctrl-0xC0+4) bytes from offset */
                size_t count = (size_t)(ctrl - 0xC0) + 4;
                if (src_pos + 1 >= src_len) break;
                uint16_t offset = ((uint16_t)src[src_pos] << 8) | src[src_pos + 1];
                src_pos += 2;
                if (offset > dst_pos || dst_pos + count > output_size) break;
                /* Byte-by-byte copy to handle overlapping */
                for (size_t i = 0; i < count; i++) {
                    output[dst_pos + i] = output[dst_pos - offset + i];
                }
                dst_pos += count;
            }
        }
        
        return (int)dst_pos;
    }
    
    memcpy(output, data + result->data_offset, result->data_size);
    return (int)result->data_size;
}

int dc_extract_tag_data(const uint8_t *data, size_t size,
                         const dc_analysis_result_t *result,
                         uint8_t *output, size_t output_size) {
    if (!data || !result || !output) return -1;
    
    if (result->tag_size == 0) {
        return 0;  /* No tag data */
    }
    
    if (result->tag_offset + result->tag_size > size) {
        return -2;
    }
    
    if (output_size < result->tag_size) {
        return -3;
    }
    
    memcpy(output, data + result->tag_offset, result->tag_size);
    return (int)result->tag_size;
}

int macbinary_unwrap(const uint8_t *data, size_t size,
                      uint8_t *data_fork, size_t data_fork_size,
                      uint8_t *rsrc_fork, size_t rsrc_fork_size) {
    if (!data || size < MACBINARY_HEADER_SIZE) return -1;
    
    dc_analysis_result_t result;
    if (macbinary_parse_header(data, size, &result) < 0) {
        return -2;
    }
    
    /* Extract data fork */
    if (data_fork && result.mb_data_fork_len > 0) {
        if (data_fork_size < result.mb_data_fork_len) {
            return -3;
        }
        if (MACBINARY_HEADER_SIZE + result.mb_data_fork_len > size) {
            return -4;
        }
        memcpy(data_fork, data + MACBINARY_HEADER_SIZE, result.mb_data_fork_len);
    }
    
    /* Extract resource fork */
    if (rsrc_fork && result.mb_rsrc_fork_len > 0) {
        if (rsrc_fork_size < result.mb_rsrc_fork_len) {
            return -5;
        }
        if (result.rsrc_offset + result.mb_rsrc_fork_len > size) {
            return -6;
        }
        memcpy(rsrc_fork, data + result.rsrc_offset, result.mb_rsrc_fork_len);
    }
    
    return 0;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char *dc_format_description(dc_disk_format_t format) {
    switch (format) {
        case DC_FORMAT_GCR_400K:  return "400K GCR (Mac 400K)";
        case DC_FORMAT_GCR_800K:  return "800K GCR (Mac 800K)";
        case DC_FORMAT_MFM_720K:  return "720K MFM (PC/Mac 720K)";
        case DC_FORMAT_MFM_1440K: return "1.44MB MFM (PC/Mac HD)";
        case DC_FORMAT_CUSTOM:    return "Custom format";
        default:                  return "Unknown format";
    }
}

const char *dc_type_description(dc_image_type_t type) {
    switch (type) {
        case DC_TYPE_DC42:    return "Disk Copy 4.2";
        case DC_TYPE_NDIF:    return "NDIF (Disk Copy 6.x)";
        case DC_TYPE_UDIF:    return "UDIF (.dmg)";
        case DC_TYPE_RAW:     return "Raw disk image";
        case DC_TYPE_SMI:     return "Self-Mounting Image";
        default:              return "Unknown";
    }
}

uint32_t dc_expected_size(dc_disk_format_t format) {
    switch (format) {
        case DC_FORMAT_GCR_400K:  return DC_SIZE_400K;
        case DC_FORMAT_GCR_800K:  return DC_SIZE_800K;
        case DC_FORMAT_MFM_720K:  return DC_SIZE_720K;
        case DC_FORMAT_MFM_1440K: return DC_SIZE_1440K;
        default:                  return 0;
    }
}

dc_disk_format_t dc_format_from_size(uint32_t data_size) {
    if (data_size == DC_SIZE_400K)  return DC_FORMAT_GCR_400K;
    if (data_size == DC_SIZE_800K)  return DC_FORMAT_GCR_800K;
    if (data_size == DC_SIZE_720K)  return DC_FORMAT_MFM_720K;
    if (data_size == DC_SIZE_1440K) return DC_FORMAT_MFM_1440K;
    return DC_FORMAT_CUSTOM;
}

int dc_generate_report(const dc_analysis_result_t *result,
                        char *buffer, size_t buffer_size) {
    if (!result || !buffer || buffer_size == 0) return 0;
    
    int written = 0;
    
    written += snprintf(buffer + written, buffer_size - written,
        "╔══════════════════════════════════════════════════════════════════╗\n"
        "║              DISK COPY IMAGE ANALYSIS REPORT                    ║\n"
        "╚══════════════════════════════════════════════════════════════════╝\n\n");
    
    /* Format identification */
    written += snprintf(buffer + written, buffer_size - written,
        "Format: %s\n", result->format_description);
    
    written += snprintf(buffer + written, buffer_size - written,
        "Type:   %s\n", dc_type_description(result->image_type));
    
    written += snprintf(buffer + written, buffer_size - written,
        "Valid:  %s\n\n", result->is_valid ? "Yes" : "No");
    
    /* MacBinary info */
    if (result->macbinary_type != MB_TYPE_NONE) {
        written += snprintf(buffer + written, buffer_size - written,
            "MacBinary Wrapper:\n"
            "  Filename:    %s\n"
            "  Type:        %s\n"
            "  Creator:     %s\n"
            "  Data Fork:   %u bytes\n"
            "  Rsrc Fork:   %u bytes\n\n",
            result->mb_filename,
            result->mb_type,
            result->mb_creator,
            result->mb_data_fork_len,
            result->mb_rsrc_fork_len);
    }
    
    /* Volume info */
    if (result->volume_name[0]) {
        written += snprintf(buffer + written, buffer_size - written,
            "Volume Information:\n"
            "  Name:        %s\n"
            "  Disk Format: %s\n"
            "  Sectors:     %u\n"
            "  Data Size:   %u bytes (%u KB)\n",
            result->volume_name,
            dc_format_description(result->disk_format),
            result->sector_count,
            result->data_size, result->data_size / 1024);
        
        if (result->tag_size > 0) {
            written += snprintf(buffer + written, buffer_size - written,
                "  Tag Size:    %u bytes\n", result->tag_size);
        }
        written += snprintf(buffer + written, buffer_size - written, "\n");
    }
    
    /* Checksum info */
    if (result->image_type == DC_TYPE_DC42) {
        written += snprintf(buffer + written, buffer_size - written,
            "Checksums:\n"
            "  Data:     0x%08X (stored)\n"
            "  Tag:      0x%08X (stored)\n",
            result->data_checksum,
            result->tag_checksum);
        
        if (result->calculated_checksum != 0) {
            written += snprintf(buffer + written, buffer_size - written,
                "  Calc:     0x%08X %s\n",
                result->calculated_checksum,
                result->checksum_valid ? "(valid)" : "(MISMATCH!)");
        }
        written += snprintf(buffer + written, buffer_size - written, "\n");
    }
    
    /* SMI info */
    if (result->has_stub) {
        written += snprintf(buffer + written, buffer_size - written,
            "Self-Mounting Image:\n"
            "  Stub Size:   %u bytes (0x%X)\n\n",
            result->stub_size, result->stub_size);
    }
    
    /* NDIF info */
    if (result->image_type == DC_TYPE_NDIF) {
        written += snprintf(buffer + written, buffer_size - written,
            "NDIF Properties:\n"
            "  Compressed:  %s\n"
            "  Blocks:      %u\n\n",
            result->is_compressed ? "Yes (ADC)" : "No",
            result->block_count);
    }
    
    /* Offsets */
    written += snprintf(buffer + written, buffer_size - written,
        "File Offsets:\n"
        "  Header:      0x%08X\n"
        "  Data:        0x%08X\n",
        result->header_offset,
        result->data_offset);
    
    if (result->tag_offset > 0 && result->tag_size > 0) {
        written += snprintf(buffer + written, buffer_size - written,
            "  Tags:        0x%08X\n", result->tag_offset);
    }
    
    if (result->rsrc_offset > 0) {
        written += snprintf(buffer + written, buffer_size - written,
            "  Resources:   0x%08X\n", result->rsrc_offset);
    }
    
    return written;
}

/* ============================================================================
 * Image Creation
 * ============================================================================ */

int dc42_create_header(const char *volume_name,
                        dc_disk_format_t format,
                        const uint8_t *data, size_t data_size,
                        const uint8_t *tag_data, size_t tag_size,
                        dc42_header_t *header) {
    if (!volume_name || !data || !header) return -1;
    
    memset(header, 0, sizeof(dc42_header_t));
    
    /* Volume name (Pascal string) */
    size_t name_len = strlen(volume_name);
    if (name_len > 63) name_len = 63;
    header->volume_name[0] = (uint8_t)name_len;
    memcpy(header->volume_name + 1, volume_name, name_len);
    
    /* Sizes */
    write_be32((uint8_t *)&header->data_size, (uint32_t)data_size);
    write_be32((uint8_t *)&header->tag_size, (uint32_t)tag_size);
    
    /* Calculate checksums */
    uint32_t data_cksum = dc_calculate_checksum(data, data_size);
    write_be32((uint8_t *)&header->data_checksum, data_cksum);
    
    if (tag_data && tag_size > 0) {
        uint32_t tag_cksum = dc_calculate_checksum(tag_data, tag_size);
        write_be32((uint8_t *)&header->tag_checksum, tag_cksum);
    }
    
    /* Format info */
    header->disk_encoding = (uint8_t)format;
    header->format_byte = 0x22;  /* Mac format */
    
    /* Magic word */
    write_be16((uint8_t *)&header->private_word, 0x0100);
    
    return 0;
}

int dc42_create_image(const char *volume_name,
                       dc_disk_format_t format,
                       const uint8_t *data, size_t data_size,
                       uint8_t *output, size_t output_size) {
    if (!volume_name || !data || !output) return -1;
    
    size_t required = DC42_HEADER_SIZE + data_size;
    if (output_size < required) return -2;
    
    /* Create header */
    dc42_header_t header;
    if (dc42_create_header(volume_name, format, data, data_size,
                            NULL, 0, &header) < 0) {
        return -3;
    }
    
    /* Write header */
    memcpy(output, &header, DC42_HEADER_SIZE);
    
    /* Write data */
    memcpy(output + DC42_HEADER_SIZE, data, data_size);
    
    return (int)required;
}

/* ============================================================================
 * NDIF Functions
 * ============================================================================ */

bool ndif_is_compressed(const uint8_t *data, size_t size,
                         const dc_analysis_result_t *result) {
    if (!data || !result) return false;
    
    /* Check resource fork for compression indicators */
    if (result->rsrc_offset > 0 && result->mb_rsrc_fork_len > 0) {
        /* Look for 'bcem' resource which indicates block compression map */
        /* If bcem has non-zero compression types, image is compressed */
        
        /* For now, assume compressed if NDIF with resource fork */
        return true;
    }
    
    return false;
}

int adc_decompress(const uint8_t *input, size_t input_size,
                    uint8_t *output, size_t output_size) {
    if (!input || !output) return -1;
    
    /* ADC (Apple Data Compression) decompression
     * Simple LZ-style compression:
     * - Literal bytes: 0x00-0x7F followed by 1-128 literal bytes
     * - Short match: 0x80-0xBF (length 3-6, offset 0-1023)
     * - Long match: 0xC0-0xFF (length 4-67, offset 0-65535)
     */
    
    size_t in_pos = 0;
    size_t out_pos = 0;
    
    while (in_pos < input_size && out_pos < output_size) {
        uint8_t ctrl = input[in_pos++];
        
        if (ctrl < 0x80) {
            /* Literal run: 1-128 bytes */
            size_t count = (ctrl & 0x7F) + 1;
            
            if (in_pos + count > input_size) return -2;
            if (out_pos + count > output_size) return -3;
            
            memcpy(output + out_pos, input + in_pos, count);
            in_pos += count;
            out_pos += count;
            
        } else if (ctrl < 0xC0) {
            /* Short match: length 3-6, offset 0-1023 */
            if (in_pos >= input_size) return -4;
            
            uint8_t byte2 = input[in_pos++];
            size_t length = ((ctrl >> 2) & 0x0F) + 3;
            size_t offset = ((ctrl & 0x03) << 8) | byte2;
            
            if (offset > out_pos) return -5;
            if (out_pos + length > output_size) return -6;
            
            /* Copy from previous output */
            for (size_t i = 0; i < length; i++) {
                output[out_pos + i] = output[out_pos - offset + i];
            }
            out_pos += length;
            
        } else {
            /* Long match: length 4-67, offset 0-65535 */
            if (in_pos + 2 > input_size) return -7;
            
            size_t length = (ctrl & 0x3F) + 4;
            size_t offset = (input[in_pos] << 8) | input[in_pos + 1];
            in_pos += 2;
            
            if (offset > out_pos) return -8;
            if (out_pos + length > output_size) return -9;
            
            for (size_t i = 0; i < length; i++) {
                output[out_pos + i] = output[out_pos - offset + i];
            }
            out_pos += length;
        }
    }
    
    return (int)out_pos;
}

/* ============================================================================
 * SMI Functions
 * ============================================================================ */

uint32_t smi_detect_stub(const uint8_t *data, size_t size) {
    if (!data || size < 0x100) return 0;
    
    /* SMI stub characteristics:
     * - Starts with 68K code (common: 4E56 = LINK, 4E71 = NOP)
     * - Contains CODE resource at 0x80-0x400 range
     * - Disk image header typically at 0x400-0x600
     */
    
    /* Look for DC42 header signature after common stub sizes */
    uint32_t stub_sizes[] = { 0x400, 0x480, 0x500, 0x580, 0x600, 0x680, 0x700, 0x800 };
    
    for (size_t i = 0; i < sizeof(stub_sizes) / sizeof(stub_sizes[0]); i++) {
        uint32_t offset = stub_sizes[i];
        
        if (offset + DC42_HEADER_SIZE > size) continue;
        
        /* Check for DC42 magic at this offset */
        uint8_t name_len = data[offset];
        if (name_len > 0 && name_len <= 63) {
            uint16_t magic = read_be16(data + offset + 82);
            if (magic == 0x0100) {
                return offset;
            }
        }
        
        /* Check for HFS signature 'BD' which might indicate disk data */
        if (data[offset] == 'B' && data[offset + 1] == 'D') {
            /* Might be start of disk data with visible MDB */
            return offset;
        }
    }
    
    /* Default stub size for unknown SMI */
    return 0x400;
}

int smi_extract_image(const uint8_t *data, size_t size,
                       uint8_t *output, size_t output_size) {
    if (!data || !output || size < 0x100) return -1;
    
    uint32_t stub_size = smi_detect_stub(data, size);
    
    if (stub_size >= size) return -2;
    
    /* Try to parse as DC42 after stub */
    dc_analysis_result_t result;
    if (dc42_parse_header(data + stub_size, size - stub_size, &result) == 0) {
        /* Extract disk data */
        return dc_extract_disk_data(data + stub_size, size - stub_size,
                                     &result, output, output_size);
    }
    
    /* If not DC42, extract raw data after stub */
    size_t data_size = size - stub_size;
    if (output_size < data_size) return -3;
    
    memcpy(output, data + stub_size, data_size);
    return (int)data_size;
}
