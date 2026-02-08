#ifndef UFT_ATARI_XEX_H
#define UFT_ATARI_XEX_H

/**
 * @file uft_atari_xex.h
 * @brief Atari 8-bit XEX Executable Format
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * XEX is the standard executable format for Atari 8-bit computers
 * (400/800/XL/XE series).
 *
 * Structure:
 * - Optional header marker: 0xFFFF
 * - One or more segments:
 *   - Start address (2 bytes, LE)
 *   - End address (2 bytes, LE)
 *   - Data bytes (end - start + 1 bytes)
 * - Segments can overlap (later segment overwrites)
 *
 * Special Addresses:
 * - RUNAD ($02E0-$02E1): Run address after loading
 * - INITAD ($02E2-$02E3): Init address (called after each segment)
 *
 * The loader processes segments in order, calling INITAD after
 * each segment if it's set, then jumps to RUNAD at the end.
 *
 * References:
 * - Atari OS Reference Manual
 * - De Re Atari
 * - RetroGhidra Atari8BitXexLoader.java
 *
 * SPDX-License-Identifier: MIT
 */


#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * XEX Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief Header marker */
#define UFT_XEX_HEADER_MARKER       0xFFFF

/** @brief Special addresses */
#define UFT_XEX_RUNAD               0x02E0  /**< Run address */
#define UFT_XEX_INITAD              0x02E2  /**< Init address */

/** @brief Memory map */
#define UFT_XEX_RAM_START           0x0000
#define UFT_XEX_RAM_END             0xFFFF
#define UFT_XEX_PAGE_ZERO_END       0x00FF
#define UFT_XEX_STACK_START         0x0100
#define UFT_XEX_STACK_END           0x01FF
#define UFT_XEX_OS_START            0xD800
#define UFT_XEX_OS_END              0xFFFF

/* ═══════════════════════════════════════════════════════════════════════════
 * XEX Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief XEX segment header
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t start_address;         /**< Segment start (LE) */
    uint16_t end_address;           /**< Segment end (LE) */
} uft_xex_segment_header_t;
#pragma pack(pop)

/**
 * @brief XEX segment information
 */
typedef struct {
    uint16_t start_address;
    uint16_t end_address;
    uint32_t data_offset;           /**< Offset in file to segment data */
    uint32_t data_size;             /**< Segment size in bytes */
    bool is_runad;                  /**< Segment sets RUNAD */
    bool is_initad;                 /**< Segment sets INITAD */
} uft_xex_segment_info_t;

/**
 * @brief XEX file information
 */
typedef struct {
    uint32_t file_size;
    uint32_t segment_count;
    uint16_t run_address;           /**< RUNAD value (0 if not set) */
    uint16_t init_address;          /**< Last INITAD value */
    uint16_t lowest_address;        /**< Lowest load address */
    uint16_t highest_address;       /**< Highest load address */
    bool has_runad;
    bool has_initad;
    bool has_header_marker;
    bool valid;
} uft_xex_file_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_xex_segment_header_t) == 4, "XEX segment header must be 4 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read 16-bit little-endian
 */
static inline uint16_t uft_xex_le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/**
 * @brief Check if address is RUNAD
 */
static inline bool uft_xex_is_runad(uint16_t start, uint16_t end) {
    return start <= UFT_XEX_RUNAD && end >= UFT_XEX_RUNAD + 1;
}

/**
 * @brief Check if address is INITAD
 */
static inline bool uft_xex_is_initad(uint16_t start, uint16_t end) {
    return start <= UFT_XEX_INITAD && end >= UFT_XEX_INITAD + 1;
}

/**
 * @brief Get memory region name
 */
static inline const char* uft_xex_region_name(uint16_t addr) {
    if (addr <= 0x00FF) return "Zero Page";
    if (addr <= 0x01FF) return "Stack";
    if (addr <= 0x047F) return "OS Variables";
    if (addr <= 0x057F) return "Screen Editor";
    if (addr <= 0x06FF) return "Floating Point";
    if (addr <= 0x7FFF) return "User RAM";
    if (addr <= 0x9FFF) return "Cartridge A";
    if (addr <= 0xBFFF) return "Cartridge B / RAM";
    if (addr <= 0xCFFF) return "ROM / I/O";
    if (addr <= 0xD7FF) return "Hardware I/O";
    return "OS ROM";
}

/**
 * @brief Probe for XEX format
 * @return Confidence score (0-100)
 */
static inline int uft_xex_probe(const uint8_t* data, size_t size) {
    if (!data || size < 6) return 0;  /* Minimum: header + one segment */
    
    int score = 0;
    size_t pos = 0;
    
    /* Check for header marker */
    uint16_t marker = uft_xex_le16(data);
    if (marker == UFT_XEX_HEADER_MARKER) {
        score += 30;
        pos = 2;
    }
    
    /* Validate first segment */
    if (pos + 4 > size) return 0;
    
    uint16_t start = uft_xex_le16(data + pos);
    uint16_t end = uft_xex_le16(data + pos + 2);
    
    /* Valid address range */
    if (end >= start) {
        score += 20;
        
        uint32_t seg_size = end - start + 1;
        if (pos + 4 + seg_size <= size) {
            score += 20;
        }
        
        /* Common load addresses */
        if (start >= 0x2000 && start < 0xC000) {
            score += 15;  /* User RAM */
        } else if (start == UFT_XEX_RUNAD || start == UFT_XEX_INITAD) {
            score += 10;  /* Run/Init vectors */
        }
    }
    
    /* Try to parse multiple segments */
    int segments = 0;
    pos = (marker == UFT_XEX_HEADER_MARKER) ? 2 : 0;
    
    while (pos + 4 <= size && segments < 100) {
        /* Check for additional header marker */
        uint16_t word = uft_xex_le16(data + pos);
        if (word == UFT_XEX_HEADER_MARKER) {
            pos += 2;
            if (pos + 4 > size) break;
        }
        
        start = uft_xex_le16(data + pos);
        end = uft_xex_le16(data + pos + 2);
        
        if (end < start) break;  /* Invalid */
        
        uint32_t seg_size = end - start + 1;
        pos += 4 + seg_size;
        segments++;
    }
    
    if (segments > 0) {
        score += 15;
    }
    
    return (score > 100) ? 100 : score;
}

/**
 * @brief Parse XEX file
 */
static inline bool uft_xex_parse(const uint8_t* data, size_t size,
                                  uft_xex_file_info_t* info) {
    if (!data || !info || size < 6) return false;
    
    memset(info, 0, sizeof(*info));
    info->file_size = size;
    info->lowest_address = 0xFFFF;
    info->highest_address = 0x0000;
    
    size_t pos = 0;
    
    /* Check header marker */
    uint16_t marker = uft_xex_le16(data);
    if (marker == UFT_XEX_HEADER_MARKER) {
        info->has_header_marker = true;
        pos = 2;
    }
    
    /* Parse segments */
    while (pos + 4 <= size) {
        /* Check for segment header marker */
        uint16_t word = uft_xex_le16(data + pos);
        if (word == UFT_XEX_HEADER_MARKER) {
            pos += 2;
            if (pos + 4 > size) break;
        }
        
        uint16_t start = uft_xex_le16(data + pos);
        uint16_t end = uft_xex_le16(data + pos + 2);
        
        if (end < start) break;  /* Invalid segment */
        
        uint32_t seg_size = end - start + 1;
        if (pos + 4 + seg_size > size) break;  /* Truncated */
        
        info->segment_count++;
        
        /* Track address range */
        if (start < info->lowest_address) info->lowest_address = start;
        if (end > info->highest_address) info->highest_address = end;
        
        /* Check for RUNAD */
        if (uft_xex_is_runad(start, end)) {
            size_t runad_offset = pos + 4 + (UFT_XEX_RUNAD - start);
            if (runad_offset + 2 <= size) {
                info->run_address = uft_xex_le16(data + runad_offset);
                info->has_runad = true;
            }
        }
        
        /* Check for INITAD */
        if (uft_xex_is_initad(start, end)) {
            size_t initad_offset = pos + 4 + (UFT_XEX_INITAD - start);
            if (initad_offset + 2 <= size) {
                info->init_address = uft_xex_le16(data + initad_offset);
                info->has_initad = true;
            }
        }
        
        pos += 4 + seg_size;
    }
    
    info->valid = (info->segment_count > 0);
    return info->valid;
}

/**
 * @brief Iterate segments in XEX file
 * @param offset Current offset (start with 0 or 2 if has marker)
 * @return Next offset, or 0 if no more segments
 */
static inline size_t uft_xex_next_segment(const uint8_t* data, size_t size,
                                           size_t offset, uft_xex_segment_info_t* seg) {
    if (!data || !seg || offset + 4 > size) return 0;
    
    /* Skip header marker if present */
    uint16_t word = uft_xex_le16(data + offset);
    if (word == UFT_XEX_HEADER_MARKER) {
        offset += 2;
        if (offset + 4 > size) return 0;
    }
    
    seg->start_address = uft_xex_le16(data + offset);
    seg->end_address = uft_xex_le16(data + offset + 2);
    
    if (seg->end_address < seg->start_address) return 0;
    
    seg->data_size = seg->end_address - seg->start_address + 1;
    seg->data_offset = offset + 4;
    
    if (seg->data_offset + seg->data_size > size) return 0;
    
    seg->is_runad = uft_xex_is_runad(seg->start_address, seg->end_address);
    seg->is_initad = uft_xex_is_initad(seg->start_address, seg->end_address);
    
    return seg->data_offset + seg->data_size;
}

/**
 * @brief Print XEX file info
 */
static inline void uft_xex_print_info(const uft_xex_file_info_t* info) {
    if (!info) return;
    
    printf("Atari 8-bit XEX Executable:\n");
    printf("  File Size:      %u bytes\n", info->file_size);
    printf("  Segments:       %u\n", info->segment_count);
    printf("  Address Range:  $%04X - $%04X\n", info->lowest_address, info->highest_address);
    
    if (info->has_runad) {
        printf("  Run Address:    $%04X\n", info->run_address);
    }
    if (info->has_initad) {
        printf("  Init Address:   $%04X\n", info->init_address);
    }
    
    printf("  Header Marker:  %s\n", info->has_header_marker ? "Yes" : "No");
}

/**
 * @brief List all segments
 */
static inline void uft_xex_list_segments(const uint8_t* data, size_t size) {
    if (!data) return;
    
    printf("XEX Segments:\n");
    printf("  %-4s %-11s %-6s %s\n", "#", "Address", "Size", "Notes");
    printf("  %-4s %-11s %-6s %s\n", "----", "-----------", "------", "-----");
    
    size_t offset = 0;
    
    /* Skip initial header marker */
    if (size >= 2 && uft_xex_le16(data) == UFT_XEX_HEADER_MARKER) {
        offset = 2;
    }
    
    uft_xex_segment_info_t seg;
    int num = 1;
    
    while ((offset = uft_xex_next_segment(data, size, offset, &seg)) != 0) {
        printf("  %-4d $%04X-$%04X %-6u %s%s\n",
               num++,
               seg.start_address, seg.end_address,
               seg.data_size,
               seg.is_runad ? "RUNAD " : "",
               seg.is_initad ? "INITAD" : "");
    }
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_ATARI_XEX_H */
