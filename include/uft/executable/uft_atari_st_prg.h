/**
 * @file uft_atari_st_prg.h
 * @brief Atari ST PRG/TOS Executable Format
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * PRG is the standard executable format for Atari ST (68000).
 * Also known as GEMDOS or TOS executable format.
 *
 * Header (28 bytes):
 * - Magic: 0x601A (bra.s +26) or 0x601B (bra.s +27)
 * - TEXT size (4 bytes)
 * - DATA size (4 bytes)
 * - BSS size (4 bytes)
 * - Symbol table size (4 bytes)
 * - Reserved (4 bytes)
 * - Flags (4 bytes) - bit 0: no relocation if clear
 *
 * Sections:
 * - TEXT: Executable code
 * - DATA: Initialized data
 * - BSS: Uninitialized data (not in file)
 * - Symbols: Optional symbol table
 * - Relocation: Fixup table
 *
 * Relocation Format:
 * - First 4 bytes: Offset to first relocation
 * - Following bytes: Relative offsets (0 = skip 254 bytes)
 *
 * References:
 * - Atari ST Internals
 * - GEMDOS Reference Manual
 * - RetroGhidra AtariStLoader.java
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_ATARI_ST_PRG_H
#define UFT_ATARI_ST_PRG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * PRG Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief Magic numbers */
#define UFT_ST_PRG_MAGIC_601A       0x601A  /**< bra.s +26 */
#define UFT_ST_PRG_MAGIC_601B       0x601B  /**< bra.s +27 */

/** @brief Header size */
#define UFT_ST_PRG_HEADER_SIZE      28      /**< 0x1C bytes */

/** @brief Header offsets */
#define UFT_ST_PRG_OFF_MAGIC        0x00    /**< Branch opcode */
#define UFT_ST_PRG_OFF_TEXT_SIZE    0x02    /**< TEXT segment size */
#define UFT_ST_PRG_OFF_DATA_SIZE    0x06    /**< DATA segment size */
#define UFT_ST_PRG_OFF_BSS_SIZE     0x0A    /**< BSS segment size */
#define UFT_ST_PRG_OFF_SYM_SIZE     0x0E    /**< Symbol table size */
#define UFT_ST_PRG_OFF_RESERVED     0x12    /**< Reserved */
#define UFT_ST_PRG_OFF_FLAGS        0x16    /**< Flags */
#define UFT_ST_PRG_OFF_RELOC_FLAG   0x1A    /**< Relocation flag (unused) */

/** @brief Flags */
#define UFT_ST_PRG_FLAG_FASTLOAD    0x0001  /**< Don't clear heap */
#define UFT_ST_PRG_FLAG_TTRAMONLY   0x0002  /**< Load into TT RAM only */
#define UFT_ST_PRG_FLAG_TTRAMMEM    0x0004  /**< Malloc from TT RAM */
#define UFT_ST_PRG_FLAG_SHARED_TEXT 0x0800  /**< Shared text segment */
#define UFT_ST_PRG_FLAG_RELOCED     0x8000  /**< Already relocated */

/** @brief Default load address */
#define UFT_ST_PRG_LOAD_ADDRESS     0x00010000  /**< Arbitrary base */

/* ═══════════════════════════════════════════════════════════════════════════
 * PRG Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief PRG header (28 bytes, Big Endian!)
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t magic;             /**< Branch opcode (0x601A or 0x601B) */
    uint32_t text_size;         /**< TEXT segment size (BE) */
    uint32_t data_size;         /**< DATA segment size (BE) */
    uint32_t bss_size;          /**< BSS segment size (BE) */
    uint32_t symbol_size;       /**< Symbol table size (BE) */
    uint32_t reserved;          /**< Reserved (BE) */
    uint32_t flags;             /**< PRGFLAGS (BE) */
    uint16_t reloc_flag;        /**< Relocation flag (unused) */
} uft_st_prg_header_t;
#pragma pack(pop)

/**
 * @brief DRI symbol table entry (14 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    char name[8];               /**< Symbol name (null padded) */
    uint16_t type;              /**< Symbol type (BE) */
    uint32_t value;             /**< Symbol value (BE) */
} uft_st_prg_symbol_t;
#pragma pack(pop)

/**
 * @brief PRG file information
 */
typedef struct {
    uint16_t magic;
    uint32_t text_size;
    uint32_t data_size;
    uint32_t bss_size;
    uint32_t symbol_size;
    uint32_t flags;
    uint32_t text_offset;       /**< Offset to TEXT in file */
    uint32_t data_offset;       /**< Offset to DATA in file */
    uint32_t symbol_offset;     /**< Offset to symbols in file */
    uint32_t reloc_offset;      /**< Offset to relocation in file */
    uint32_t total_memory;      /**< TEXT + DATA + BSS */
    uint32_t file_size;
    uint32_t symbol_count;
    bool has_relocation;
    bool valid;
} uft_st_prg_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_st_prg_header_t) == 28, "ST PRG header must be 28 bytes");
_Static_assert(sizeof(uft_st_prg_symbol_t) == 14, "ST PRG symbol must be 14 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions - Big Endian
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read 16-bit big-endian
 */
static inline uint16_t uft_st_prg_be16(const uint8_t* p) {
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

/**
 * @brief Read 32-bit big-endian
 */
static inline uint32_t uft_st_prg_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/**
 * @brief Get flag description
 */
static inline const char* uft_st_prg_flag_desc(uint32_t flags) {
    static char buf[128];
    buf[0] = '\0';
    
    if (flags & UFT_ST_PRG_FLAG_FASTLOAD)    strcat(buf, "Fastload ");
    if (flags & UFT_ST_PRG_FLAG_TTRAMONLY)   strcat(buf, "TT-RAM-Only ");
    if (flags & UFT_ST_PRG_FLAG_TTRAMMEM)    strcat(buf, "TT-Malloc ");
    if (flags & UFT_ST_PRG_FLAG_SHARED_TEXT) strcat(buf, "Shared-Text ");
    if (flags & UFT_ST_PRG_FLAG_RELOCED)     strcat(buf, "Relocated ");
    
    if (buf[0] == '\0') strcpy(buf, "None");
    return buf;
}

/**
 * @brief Probe for PRG format
 * @return Confidence score (0-100)
 */
static inline int uft_st_prg_probe(const uint8_t* data, size_t size) {
    if (!data || size < UFT_ST_PRG_HEADER_SIZE) return 0;
    
    int score = 0;
    
    /* Check magic */
    uint16_t magic = uft_st_prg_be16(data);
    if (magic == UFT_ST_PRG_MAGIC_601A || magic == UFT_ST_PRG_MAGIC_601B) {
        score += 50;
    } else {
        return 0;
    }
    
    /* Read sizes */
    uint32_t text_size = uft_st_prg_be32(data + UFT_ST_PRG_OFF_TEXT_SIZE);
    uint32_t data_size = uft_st_prg_be32(data + UFT_ST_PRG_OFF_DATA_SIZE);
    uint32_t bss_size = uft_st_prg_be32(data + UFT_ST_PRG_OFF_BSS_SIZE);
    uint32_t sym_size = uft_st_prg_be32(data + UFT_ST_PRG_OFF_SYM_SIZE);
    
    /* Validate sizes */
    uint32_t expected_min = UFT_ST_PRG_HEADER_SIZE + text_size + data_size;
    if (expected_min <= size) {
        score += 25;
    }
    
    /* Symbol table should be multiple of 14 */
    if (sym_size == 0 || (sym_size % 14) == 0) {
        score += 10;
    }
    
    /* BSS should be reasonable */
    if (bss_size < 0x1000000) {  /* Less than 16MB */
        score += 10;
    }
    
    /* Check flags for valid bits */
    uint32_t flags = uft_st_prg_be32(data + UFT_ST_PRG_OFF_FLAGS);
    uint32_t known_flags = UFT_ST_PRG_FLAG_FASTLOAD | UFT_ST_PRG_FLAG_TTRAMONLY |
                           UFT_ST_PRG_FLAG_TTRAMMEM | UFT_ST_PRG_FLAG_SHARED_TEXT |
                           UFT_ST_PRG_FLAG_RELOCED;
    if ((flags & ~known_flags) == 0) {
        score += 5;
    }
    
    return (score > 100) ? 100 : score;
}

/**
 * @brief Parse PRG file
 */
static inline bool uft_st_prg_parse(const uint8_t* data, size_t size,
                                     uft_st_prg_info_t* info) {
    if (!data || !info || size < UFT_ST_PRG_HEADER_SIZE) return false;
    
    memset(info, 0, sizeof(*info));
    info->file_size = size;
    
    /* Read header */
    info->magic = uft_st_prg_be16(data + UFT_ST_PRG_OFF_MAGIC);
    info->text_size = uft_st_prg_be32(data + UFT_ST_PRG_OFF_TEXT_SIZE);
    info->data_size = uft_st_prg_be32(data + UFT_ST_PRG_OFF_DATA_SIZE);
    info->bss_size = uft_st_prg_be32(data + UFT_ST_PRG_OFF_BSS_SIZE);
    info->symbol_size = uft_st_prg_be32(data + UFT_ST_PRG_OFF_SYM_SIZE);
    info->flags = uft_st_prg_be32(data + UFT_ST_PRG_OFF_FLAGS);
    
    /* Validate magic */
    if (info->magic != UFT_ST_PRG_MAGIC_601A && info->magic != UFT_ST_PRG_MAGIC_601B) {
        return false;
    }
    
    /* Calculate offsets */
    info->text_offset = UFT_ST_PRG_HEADER_SIZE;
    info->data_offset = info->text_offset + info->text_size;
    info->symbol_offset = info->data_offset + info->data_size;
    info->reloc_offset = info->symbol_offset + info->symbol_size;
    
    /* Total memory required */
    info->total_memory = info->text_size + info->data_size + info->bss_size;
    
    /* Symbol count */
    if (info->symbol_size > 0) {
        info->symbol_count = info->symbol_size / 14;
    }
    
    /* Check for relocation data */
    if (info->reloc_offset < size) {
        /* First longword is offset to first relocation */
        if (info->reloc_offset + 4 <= size) {
            uint32_t first_reloc = uft_st_prg_be32(data + info->reloc_offset);
            info->has_relocation = (first_reloc != 0);
        }
    }
    
    info->valid = true;
    return true;
}

/**
 * @brief Print PRG info
 */
static inline void uft_st_prg_print_info(const uft_st_prg_info_t* info) {
    if (!info) return;
    
    printf("Atari ST PRG Executable:\n");
    printf("  Magic:          0x%04X (%s)\n", info->magic,
           info->magic == UFT_ST_PRG_MAGIC_601A ? "bra.s +26" : "bra.s +27");
    printf("  TEXT Size:      %u bytes\n", info->text_size);
    printf("  DATA Size:      %u bytes\n", info->data_size);
    printf("  BSS Size:       %u bytes\n", info->bss_size);
    printf("  Symbol Size:    %u bytes (%u symbols)\n", 
           info->symbol_size, info->symbol_count);
    printf("  Total Memory:   %u bytes\n", info->total_memory);
    printf("  Flags:          0x%08X (%s)\n", info->flags, uft_st_prg_flag_desc(info->flags));
    printf("  Has Relocation: %s\n", info->has_relocation ? "Yes" : "No");
    printf("  File Size:      %u bytes\n", info->file_size);
}

/**
 * @brief Get symbol by index
 */
static inline bool uft_st_prg_get_symbol(const uint8_t* data, size_t size,
                                          const uft_st_prg_info_t* info,
                                          uint32_t index,
                                          char* name, uint16_t* type, uint32_t* value) {
    if (!data || !info || index >= info->symbol_count) return false;
    
    uint32_t sym_offset = info->symbol_offset + index * 14;
    if (sym_offset + 14 > size) return false;
    
    const uft_st_prg_symbol_t* sym = (const uft_st_prg_symbol_t*)(data + sym_offset);
    
    if (name) {
        memcpy(name, sym->name, 8);
        name[8] = '\0';
    }
    if (type) *type = uft_st_prg_be16((const uint8_t*)&sym->type);
    if (value) *value = uft_st_prg_be32((const uint8_t*)&sym->value);
    
    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_ATARI_ST_PRG_H */
