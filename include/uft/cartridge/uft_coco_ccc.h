/**
 * @file uft_coco_ccc.h
 * @brief Tandy Color Computer (CoCo) Cartridge Format
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * CCC is the cartridge ROM format for Tandy Color Computer (CoCo).
 * These are raw ROM dumps with no header - just the ROM data.
 *
 * Characteristics:
 * - No magic number or header
 * - Sizes: 2K, 4K, 8K, 16K, 32K
 * - Load address: $C000 (49152)
 * - CPU: Motorola 6809 (Big Endian)
 *
 * Auto-Start Detection:
 * - Cold start vector at $C000 (first 2 bytes)
 * - Should point into cartridge space ($C000-$FEFF)
 *
 * Extended CoCo 3:
 * - Can have larger cartridges with bank switching
 * - Super Extended BASIC cartridges
 *
 * Memory Map:
 * - $0000-$7FFF: RAM (32K standard CoCo)
 * - $8000-$9FFF: Extended BASIC ROM
 * - $A000-$BFFF: Color BASIC ROM
 * - $C000-$FEFF: Cartridge ROM
 * - $FF00-$FFEF: I/O registers
 * - $FFF0-$FFFF: Vectors (mirrored from ROM)
 *
 * References:
 * - CoCo Technical Reference Manual
 * - RetroGhidra CocoCccLoader.java
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_COCO_CCC_H
#define UFT_COCO_CCC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * CoCo CCC Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief Standard ROM sizes */
#define UFT_COCO_CCC_SIZE_2K        2048
#define UFT_COCO_CCC_SIZE_4K        4096
#define UFT_COCO_CCC_SIZE_8K        8192
#define UFT_COCO_CCC_SIZE_16K       16384
#define UFT_COCO_CCC_SIZE_32K       32768

/** @brief Memory map */
#define UFT_COCO_CCC_LOAD_ADDRESS   0xC000  /**< Cartridge load address */
#define UFT_COCO_CCC_END_ADDRESS    0xFEFF  /**< Cartridge end address */
#define UFT_COCO_CCC_MAX_SIZE       (0xFEFF - 0xC000 + 1)  /**< 15.75K max */

/** @brief CoCo memory regions */
#define UFT_COCO_RAM_START          0x0000
#define UFT_COCO_RAM_END            0x7FFF
#define UFT_COCO_EXT_BASIC_START    0x8000
#define UFT_COCO_EXT_BASIC_END      0x9FFF
#define UFT_COCO_BASIC_START        0xA000
#define UFT_COCO_BASIC_END          0xBFFF
#define UFT_COCO_CART_START         0xC000
#define UFT_COCO_CART_END           0xFEFF
#define UFT_COCO_IO_START           0xFF00
#define UFT_COCO_IO_END             0xFFEF
#define UFT_COCO_VECTOR_START       0xFFF0
#define UFT_COCO_VECTOR_END         0xFFFF

/* ═══════════════════════════════════════════════════════════════════════════
 * CoCo CCC Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief CoCo cartridge information
 */
typedef struct {
    uint32_t file_size;
    uint16_t load_address;
    uint16_t end_address;
    uint16_t entry_point;       /**< From first 2 bytes (cold start vector) */
    uint32_t rom_size;
    bool is_standard_size;      /**< 2K, 4K, 8K, 16K, or 32K */
    bool has_valid_entry;       /**< Entry point in valid range */
    bool valid;
} uft_coco_ccc_info_t;

/**
 * @brief 6809 vector table (at end of ROM if mirrored)
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t reserved[6];       /**< $FFF0-$FFFB: Reserved */
    uint16_t swi3;              /**< $FFFC: SWI3 vector */
    uint16_t swi2;              /**< $FFFE: SWI2 vector */
} uft_coco_vectors_partial_t;
#pragma pack(pop)

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read 16-bit big-endian (6809 is big-endian)
 */
static inline uint16_t uft_coco_be16(const uint8_t* p) {
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

/**
 * @brief Get ROM size name
 */
static inline const char* uft_coco_ccc_size_name(uint32_t size) {
    switch (size) {
        case UFT_COCO_CCC_SIZE_2K:  return "2K";
        case UFT_COCO_CCC_SIZE_4K:  return "4K";
        case UFT_COCO_CCC_SIZE_8K:  return "8K";
        case UFT_COCO_CCC_SIZE_16K: return "16K";
        case UFT_COCO_CCC_SIZE_32K: return "32K";
        default:                   return "Non-standard";
    }
}

/**
 * @brief Check if size is standard
 */
static inline bool uft_coco_ccc_is_standard_size(uint32_t size) {
    return (size == UFT_COCO_CCC_SIZE_2K ||
            size == UFT_COCO_CCC_SIZE_4K ||
            size == UFT_COCO_CCC_SIZE_8K ||
            size == UFT_COCO_CCC_SIZE_16K ||
            size == UFT_COCO_CCC_SIZE_32K);
}

/**
 * @brief Probe for CoCo CCC format
 * @return Confidence score (0-100)
 * 
 * Note: CCC has no magic number, so detection is heuristic-based.
 * This format should be probed with LOW priority after formats with magic.
 */
static inline int uft_coco_ccc_probe(const uint8_t* data, size_t size) {
    if (!data || size < 2) return 0;
    
    int score = 0;
    
    /* Check standard ROM sizes */
    if (uft_coco_ccc_is_standard_size(size)) {
        score += 25;
    } else if (size <= UFT_COCO_CCC_MAX_SIZE && size >= 256) {
        score += 10;  /* Non-standard but possible */
    } else {
        return 0;  /* Too large or too small */
    }
    
    /* Check entry point (first 2 bytes = cold start vector) */
    uint16_t entry = uft_coco_be16(data);
    
    /* Entry should point into cartridge space */
    if (entry >= UFT_COCO_CCC_LOAD_ADDRESS && 
        entry <= UFT_COCO_CCC_LOAD_ADDRESS + size - 1) {
        score += 30;
    } else if (entry >= 0x8000 && entry <= 0xBFFF) {
        score += 10;  /* Points to BASIC ROM (unusual but possible) */
    }
    
    /* Check for typical 6809 code patterns at entry */
    if (entry >= UFT_COCO_CCC_LOAD_ADDRESS && entry < UFT_COCO_CCC_LOAD_ADDRESS + size) {
        size_t entry_offset = entry - UFT_COCO_CCC_LOAD_ADDRESS;
        if (entry_offset + 2 <= size) {
            uint8_t opcode = data[entry_offset];
            
            /* Common 6809 startup opcodes */
            if (opcode == 0x10 ||   /* Page 2 prefix */
                opcode == 0x11 ||   /* Page 3 prefix */
                opcode == 0x1C ||   /* ANDCC (disable IRQ) */
                opcode == 0x1F ||   /* TFR */
                opcode == 0x7E ||   /* JMP */
                opcode == 0x8E ||   /* LDX immediate */
                opcode == 0xBD ||   /* JSR extended */
                opcode == 0xCC ||   /* LDD immediate */
                opcode == 0xCE ||   /* LDU immediate */
                opcode == 0x20 ||   /* BRA */
                opcode == 0x16) {   /* LBRA */
                score += 20;
            }
        }
    }
    
    /* Check ROM isn't all 0xFF (blank EPROM) or 0x00 */
    int non_ff = 0;
    int non_00 = 0;
    size_t check_len = (size > 256) ? 256 : size;
    for (size_t i = 0; i < check_len; i++) {
        if (data[i] != 0xFF) non_ff++;
        if (data[i] != 0x00) non_00++;
    }
    
    if (non_ff > check_len / 2 && non_00 > check_len / 2) {
        score += 15;  /* Has real data */
    }
    
    /* Look for typical CoCo cartridge strings */
    for (size_t i = 0; i + 5 <= size; i++) {
        if (memcmp(data + i, "COCO", 4) == 0 ||
            memcmp(data + i, "TRS-80", 6) == 0 ||
            memcmp(data + i, "TANDY", 5) == 0 ||
            memcmp(data + i, "COLOR", 5) == 0) {
            score += 10;
            break;
        }
    }
    
    return (score > 100) ? 100 : score;
}

/**
 * @brief Parse CoCo CCC cartridge
 */
static inline bool uft_coco_ccc_parse(const uint8_t* data, size_t size,
                                       uft_coco_ccc_info_t* info) {
    if (!data || !info || size < 2) return false;
    
    memset(info, 0, sizeof(*info));
    
    info->file_size = size;
    info->rom_size = size;
    info->load_address = UFT_COCO_CCC_LOAD_ADDRESS;
    info->end_address = UFT_COCO_CCC_LOAD_ADDRESS + size - 1;
    info->is_standard_size = uft_coco_ccc_is_standard_size(size);
    
    /* Extract entry point from first 2 bytes */
    info->entry_point = uft_coco_be16(data);
    
    /* Check if entry point is valid */
    info->has_valid_entry = (info->entry_point >= info->load_address &&
                              info->entry_point <= info->end_address);
    
    info->valid = true;
    return true;
}

/**
 * @brief Print CoCo CCC info
 */
static inline void uft_coco_ccc_print_info(const uft_coco_ccc_info_t* info) {
    if (!info) return;
    
    printf("Tandy Color Computer Cartridge:\n");
    printf("  ROM Size:       %u bytes (%s)\n", 
           info->rom_size, uft_coco_ccc_size_name(info->rom_size));
    printf("  Load Address:   $%04X\n", info->load_address);
    printf("  End Address:    $%04X\n", info->end_address);
    printf("  Entry Point:    $%04X %s\n", info->entry_point,
           info->has_valid_entry ? "(valid)" : "(invalid!)");
    printf("  Standard Size:  %s\n", info->is_standard_size ? "Yes" : "No");
}

/**
 * @brief Get memory region name for address
 */
static inline const char* uft_coco_region_name(uint16_t addr) {
    if (addr <= UFT_COCO_RAM_END) return "RAM";
    if (addr <= UFT_COCO_EXT_BASIC_END) return "Extended BASIC";
    if (addr <= UFT_COCO_BASIC_END) return "Color BASIC";
    if (addr <= UFT_COCO_CART_END) return "Cartridge";
    if (addr <= UFT_COCO_IO_END) return "I/O Registers";
    return "Vectors";
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_COCO_CCC_H */
