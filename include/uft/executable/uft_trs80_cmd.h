/**
 * @file uft_trs80_cmd.h
 * @brief TRS-80 /CMD Executable Format
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * /CMD is the standard executable format for TRS-80 Model I/III/4.
 * Uses a record-based structure with different record types.
 *
 * Record Structure:
 * - Type byte (1 byte)
 * - Length byte (1 byte) - special encoding!
 * - Data (variable)
 *
 * Length Encoding:
 * - Value 0, 1, 2: Actual length = value + 256 (256, 257, 258)
 * - Value >= 3: Actual length = value
 *
 * Record Types:
 * - 0x01: Object code (data block)
 * - 0x02: Transfer address (entry point)
 * - 0x04: End of file
 * - 0x05: Header/comment
 * - 0x06: PDS member entry
 * - 0x07: Patch name block
 * - 0x08: ISAM directory entry
 * - 0x0A: End of ISAM directory
 * - 0x0C: PDS directory entry
 * - 0x0E: End of PDS directory
 * - 0x10: Yank block
 * - 0x1F: Copyright block
 *
 * Object Code Record (0x01):
 * - Length byte
 * - Load address (2 bytes, LE)
 * - Data bytes (length - 2)
 *
 * Transfer Record (0x02):
 * - Length = 2 (always)
 * - Entry point address (2 bytes, LE)
 *
 * References:
 * - TRS-80 Technical Reference Manual
 * - LDOS Technical Manual
 * - RetroGhidra Trs80Loader.java
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_TRS80_CMD_H
#define UFT_TRS80_CMD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * TRS-80 CMD Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief Record types */
#define UFT_TRS80_REC_OBJECT        0x01    /**< Object code */
#define UFT_TRS80_REC_TRANSFER      0x02    /**< Transfer/entry address */
#define UFT_TRS80_REC_END           0x04    /**< End of file */
#define UFT_TRS80_REC_HEADER        0x05    /**< Header/comment */
#define UFT_TRS80_REC_MEMBER        0x06    /**< PDS member entry */
#define UFT_TRS80_REC_PATCH         0x07    /**< Patch name block */
#define UFT_TRS80_REC_ISAM          0x08    /**< ISAM directory entry */
#define UFT_TRS80_REC_END_ISAM      0x0A    /**< End of ISAM directory */
#define UFT_TRS80_REC_PDS           0x0C    /**< PDS directory entry */
#define UFT_TRS80_REC_END_PDS       0x0E    /**< End of PDS directory */
#define UFT_TRS80_REC_YANK          0x10    /**< Yank block */
#define UFT_TRS80_REC_COPYRIGHT     0x1F    /**< Copyright block */

/** @brief Memory map */
#define UFT_TRS80_RAM_START         0x4000  /**< User RAM start (Model I) */
#define UFT_TRS80_RAM_END           0xFFFF  /**< RAM end */
#define UFT_TRS80_ROM_START         0x0000  /**< ROM start */
#define UFT_TRS80_ROM_END           0x2FFF  /**< ROM end (Model I) */

/* ═══════════════════════════════════════════════════════════════════════════
 * TRS-80 CMD Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief CMD record header
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t type;               /**< Record type */
    uint8_t length;             /**< Encoded length */
} uft_trs80_record_header_t;
#pragma pack(pop)

/**
 * @brief CMD record information
 */
typedef struct {
    uint8_t type;               /**< Record type */
    uint16_t length;            /**< Decoded length */
    uint32_t data_offset;       /**< Offset to record data in file */
    uint16_t load_address;      /**< For object records */
    uint16_t data_length;       /**< Actual data bytes (length - 2 for object) */
} uft_trs80_record_info_t;

/**
 * @brief CMD file information
 */
typedef struct {
    uint32_t file_size;
    uint32_t record_count;
    uint32_t object_records;
    uint16_t entry_point;       /**< Transfer address */
    uint16_t lowest_address;
    uint16_t highest_address;
    uint32_t total_code_bytes;
    char header[64];            /**< Header/comment if present */
    char copyright[64];         /**< Copyright if present */
    bool has_entry_point;
    bool has_header;
    bool has_copyright;
    bool has_end;
    bool valid;
} uft_trs80_cmd_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_trs80_record_header_t) == 2, "TRS-80 record header must be 2 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read 16-bit little-endian
 */
static inline uint16_t uft_trs80_le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/**
 * @brief Decode length byte
 * Length values 0, 1, 2 actually mean 256, 257, 258
 */
static inline uint16_t uft_trs80_decode_length(uint8_t encoded) {
    if (encoded < 3) {
        return 256 + encoded;
    }
    return encoded;
}

/**
 * @brief Get record type name
 */
static inline const char* uft_trs80_record_name(uint8_t type) {
    switch (type) {
        case UFT_TRS80_REC_OBJECT:    return "Object Code";
        case UFT_TRS80_REC_TRANSFER:  return "Transfer Address";
        case UFT_TRS80_REC_END:       return "End of File";
        case UFT_TRS80_REC_HEADER:    return "Header/Comment";
        case UFT_TRS80_REC_MEMBER:    return "PDS Member";
        case UFT_TRS80_REC_PATCH:     return "Patch Name";
        case UFT_TRS80_REC_ISAM:      return "ISAM Directory";
        case UFT_TRS80_REC_END_ISAM:  return "End ISAM";
        case UFT_TRS80_REC_PDS:       return "PDS Directory";
        case UFT_TRS80_REC_END_PDS:   return "End PDS";
        case UFT_TRS80_REC_YANK:      return "Yank Block";
        case UFT_TRS80_REC_COPYRIGHT: return "Copyright";
        default:                      return "Unknown";
    }
}

/**
 * @brief Probe for TRS-80 CMD format
 * @return Confidence score (0-100)
 */
static inline int uft_trs80_cmd_probe(const uint8_t* data, size_t size) {
    if (!data || size < 4) return 0;  /* Minimum: type + len + 2-byte data */
    
    int score = 0;
    size_t pos = 0;
    int records = 0;
    int object_records = 0;
    bool found_transfer = false;
    bool found_end = false;
    
    /* Parse records */
    while (pos + 2 <= size && records < 1000) {
        uint8_t type = data[pos];
        uint8_t len_byte = data[pos + 1];
        uint16_t length = uft_trs80_decode_length(len_byte);
        
        /* Check valid record type */
        bool valid_type = (type == UFT_TRS80_REC_OBJECT ||
                          type == UFT_TRS80_REC_TRANSFER ||
                          type == UFT_TRS80_REC_END ||
                          type == UFT_TRS80_REC_HEADER ||
                          type == UFT_TRS80_REC_COPYRIGHT ||
                          type == UFT_TRS80_REC_MEMBER ||
                          type == UFT_TRS80_REC_PATCH);
        
        if (!valid_type && records == 0) {
            return 0;  /* First record must be valid */
        }
        
        if (!valid_type) break;
        
        records++;
        
        if (type == UFT_TRS80_REC_OBJECT) {
            object_records++;
            
            /* Check load address is reasonable */
            if (pos + 4 <= size) {
                uint16_t addr = uft_trs80_le16(data + pos + 2);
                if (addr >= 0x4000 && addr < 0xFF00) {
                    score += 2;  /* Valid user RAM address */
                }
            }
        } else if (type == UFT_TRS80_REC_TRANSFER) {
            found_transfer = true;
            if (length == 2) score += 10;  /* Correct length */
        } else if (type == UFT_TRS80_REC_END) {
            found_end = true;
            break;  /* End of file */
        }
        
        pos += 2 + length;
    }
    
    /* Score based on structure */
    if (records > 0) score += 20;
    if (object_records > 0) score += 20;
    if (found_transfer) score += 20;
    if (found_end) score += 20;
    
    /* File should end near last record */
    if (found_end && pos <= size && pos >= size - 4) {
        score += 10;
    }
    
    return (score > 100) ? 100 : score;
}

/**
 * @brief Parse TRS-80 CMD file
 */
static inline bool uft_trs80_cmd_parse(const uint8_t* data, size_t size,
                                        uft_trs80_cmd_info_t* info) {
    if (!data || !info || size < 4) return false;
    
    memset(info, 0, sizeof(*info));
    info->file_size = size;
    info->lowest_address = 0xFFFF;
    info->highest_address = 0x0000;
    
    size_t pos = 0;
    
    while (pos + 2 <= size) {
        uint8_t type = data[pos];
        uint8_t len_byte = data[pos + 1];
        
        /* END record is special - stop immediately */
        if (type == UFT_TRS80_REC_END) {
            info->has_end = true;
            info->record_count++;
            goto done;
        }
        
        uint16_t length = uft_trs80_decode_length(len_byte);
        
        if (pos + 2 + length > size) break;  /* Truncated */
        
        info->record_count++;
        
        switch (type) {
            case UFT_TRS80_REC_OBJECT:
                if (length >= 2) {
                    info->object_records++;
                    uint16_t addr = uft_trs80_le16(data + pos + 2);
                    uint16_t data_len = length - 2;
                    
                    if (addr < info->lowest_address) {
                        info->lowest_address = addr;
                    }
                    if (addr + data_len - 1 > info->highest_address) {
                        info->highest_address = addr + data_len - 1;
                    }
                    info->total_code_bytes += data_len;
                }
                break;
                
            case UFT_TRS80_REC_TRANSFER:
                if (length >= 2) {
                    info->entry_point = uft_trs80_le16(data + pos + 2);
                    info->has_entry_point = true;
                }
                break;
                
            case UFT_TRS80_REC_HEADER:
                if (length > 0 && length < 64) {
                    memcpy(info->header, data + pos + 2, length);
                    info->header[length] = '\0';
                    info->has_header = true;
                }
                break;
                
            case UFT_TRS80_REC_COPYRIGHT:
                if (length > 0 && length < 64) {
                    memcpy(info->copyright, data + pos + 2, length);
                    info->copyright[length] = '\0';
                    info->has_copyright = true;
                }
                break;
                
            default:
                /* Unknown record type - continue */
                break;
        }
        
        pos += 2 + length;
    }
    
done:
    info->valid = (info->record_count > 0);
    return info->valid;
}

/**
 * @brief Iterate records in CMD file
 * @return Next offset, or 0 if no more records
 */
static inline size_t uft_trs80_cmd_next_record(const uint8_t* data, size_t size,
                                                size_t offset, uft_trs80_record_info_t* rec) {
    if (!data || !rec || offset + 2 > size) return 0;
    
    rec->type = data[offset];
    rec->length = uft_trs80_decode_length(data[offset + 1]);
    rec->data_offset = offset + 2;
    
    if (rec->data_offset + rec->length > size) return 0;
    
    /* Extract load address for object records */
    if (rec->type == UFT_TRS80_REC_OBJECT && rec->length >= 2) {
        rec->load_address = uft_trs80_le16(data + rec->data_offset);
        rec->data_length = rec->length - 2;
    } else if (rec->type == UFT_TRS80_REC_TRANSFER && rec->length >= 2) {
        rec->load_address = uft_trs80_le16(data + rec->data_offset);
        rec->data_length = 0;
    } else {
        rec->load_address = 0;
        rec->data_length = rec->length;
    }
    
    /* End of file? */
    if (rec->type == UFT_TRS80_REC_END) {
        return 0;  /* No more records */
    }
    
    return rec->data_offset + rec->length;
}

/**
 * @brief Print CMD file info
 */
static inline void uft_trs80_cmd_print_info(const uft_trs80_cmd_info_t* info) {
    if (!info) return;
    
    printf("TRS-80 /CMD Executable:\n");
    printf("  File Size:      %u bytes\n", info->file_size);
    printf("  Records:        %u\n", info->record_count);
    printf("  Object Records: %u\n", info->object_records);
    printf("  Code Bytes:     %u\n", info->total_code_bytes);
    printf("  Address Range:  $%04X - $%04X\n", 
           info->lowest_address, info->highest_address);
    
    if (info->has_entry_point) {
        printf("  Entry Point:    $%04X\n", info->entry_point);
    }
    if (info->has_header) {
        printf("  Header:         \"%s\"\n", info->header);
    }
    if (info->has_copyright) {
        printf("  Copyright:      \"%s\"\n", info->copyright);
    }
    
    printf("  Has EOF Record: %s\n", info->has_end ? "Yes" : "No");
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_TRS80_CMD_H */
