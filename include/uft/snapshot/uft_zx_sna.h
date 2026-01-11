/**
 * @file uft_zx_sna.h
 * @brief ZX Spectrum SNA Snapshot Format
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * SNA is a memory snapshot format for ZX Spectrum emulators.
 * Originally used by hardware snapshot devices.
 *
 * 48K Format (49179 bytes):
 * - 27-byte header (Z80 registers)
 * - 48KB RAM (0x4000-0xFFFF)
 * - PC is on stack (must be popped)
 *
 * 128K Format (131103 or 147487 bytes):
 * - 27-byte header
 * - 48KB RAM (banks 5, 2, and current bank at 0xC000)
 * - 4-byte extension (PC + port 0x7FFD)
 * - 5 or 6 additional 16KB banks
 *
 * Memory Layout (ZX Spectrum 48K):
 * - 0x0000-0x3FFF: ROM (16KB, not in snapshot)
 * - 0x4000-0x57FF: Display memory (6144 bytes)
 * - 0x5800-0x5AFF: Attribute memory (768 bytes)
 * - 0x5B00-0xFFFF: User RAM
 *
 * References:
 * - https://worldofspectrum.org/faq/reference/formats.htm
 * - VICE documentation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_ZX_SNA_H
#define UFT_ZX_SNA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * ZX SNA Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief SNA header size */
#define UFT_ZX_SNA_HEADER_SIZE      27

/** @brief 48K snapshot size */
#define UFT_ZX_SNA_SIZE_48K         (27 + 48 * 1024)  /* 49179 */

/** @brief 128K snapshot sizes */
#define UFT_ZX_SNA_SIZE_128K_SHORT  (27 + 48 * 1024 + 4 + 5 * 16 * 1024)  /* 131103 */
#define UFT_ZX_SNA_SIZE_128K_LONG   (27 + 48 * 1024 + 4 + 6 * 16 * 1024)  /* 147487 */

/** @brief ZX Spectrum memory layout */
#define UFT_ZX_SNA_RAM_START        0x4000  /**< Start of RAM */
#define UFT_ZX_SNA_RAM_END          0x10000 /**< End of RAM */
#define UFT_ZX_SNA_DISPLAY_START    0x4000  /**< Display memory */
#define UFT_ZX_SNA_DISPLAY_SIZE     6144    /**< 32 × 192 */
#define UFT_ZX_SNA_ATTR_START       0x5800  /**< Attributes */
#define UFT_ZX_SNA_ATTR_SIZE        768     /**< 32 × 24 */

/** @brief Header field offsets */
#define UFT_ZX_SNA_OFF_I            0x00    /**< Interrupt register */
#define UFT_ZX_SNA_OFF_HL_ALT       0x01    /**< HL' */
#define UFT_ZX_SNA_OFF_DE_ALT       0x03    /**< DE' */
#define UFT_ZX_SNA_OFF_BC_ALT       0x05    /**< BC' */
#define UFT_ZX_SNA_OFF_AF_ALT       0x07    /**< AF' */
#define UFT_ZX_SNA_OFF_HL           0x09    /**< HL */
#define UFT_ZX_SNA_OFF_DE           0x0B    /**< DE */
#define UFT_ZX_SNA_OFF_BC           0x0D    /**< BC */
#define UFT_ZX_SNA_OFF_IY           0x0F    /**< IY */
#define UFT_ZX_SNA_OFF_IX           0x11    /**< IX */
#define UFT_ZX_SNA_OFF_IFF2         0x13    /**< IFF2 (bit 2) */
#define UFT_ZX_SNA_OFF_R            0x14    /**< R register */
#define UFT_ZX_SNA_OFF_AF           0x15    /**< AF */
#define UFT_ZX_SNA_OFF_SP           0x17    /**< Stack pointer */
#define UFT_ZX_SNA_OFF_INT_MODE     0x19    /**< Interrupt mode (0/1/2) */
#define UFT_ZX_SNA_OFF_BORDER       0x1A    /**< Border color (0-7) */

/** @brief Border color special values (hardware device) */
#define UFT_ZX_SNA_ROM_PAGED_SPEC   0x71    /**< Spectrum ROM paged in */
#define UFT_ZX_SNA_ROM_PAGED_INT1   0xC9    /**< Interface 1 ROM paged in */

/* ═══════════════════════════════════════════════════════════════════════════
 * ZX SNA Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief ZX SNA 48K header (27 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t i;              /**< Interrupt register */
    uint16_t hl_alt;        /**< HL' (little endian) */
    uint16_t de_alt;        /**< DE' */
    uint16_t bc_alt;        /**< BC' */
    uint16_t af_alt;        /**< AF' */
    uint16_t hl;            /**< HL */
    uint16_t de;            /**< DE */
    uint16_t bc;            /**< BC */
    uint16_t iy;            /**< IY */
    uint16_t ix;            /**< IX */
    uint8_t iff2;           /**< IFF2 (bit 2 only) */
    uint8_t r;              /**< R register */
    uint16_t af;            /**< AF */
    uint16_t sp;            /**< Stack pointer */
    uint8_t int_mode;       /**< Interrupt mode (0/1/2) */
    uint8_t border;         /**< Border color (0-7) */
} uft_zx_sna_header_t;
#pragma pack(pop)

/**
 * @brief ZX SNA 128K extension (4 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t pc;            /**< Program counter */
    uint8_t port_7ffd;      /**< Port 0x7FFD value */
    uint8_t tr_dos_rom;     /**< TR-DOS ROM paged (0/1) */
} uft_zx_sna_128k_ext_t;
#pragma pack(pop)

/**
 * @brief ZX SNA file information
 */
typedef struct {
    bool is_48k;            /**< 48K or 128K */
    bool is_128k;
    uint16_t sp;            /**< Stack pointer */
    uint16_t pc;            /**< Program counter (from stack or extension) */
    uint8_t int_mode;       /**< Interrupt mode */
    uint8_t border;         /**< Border color */
    bool iff2;              /**< Interrupt flip-flop 2 */
    uint8_t current_bank;   /**< Current bank at 0xC000 (128K) */
    uint32_t file_size;
    bool valid;
} uft_zx_sna_file_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_zx_sna_header_t) == 27, "ZX SNA header must be 27 bytes");
_Static_assert(sizeof(uft_zx_sna_128k_ext_t) == 4, "ZX SNA 128K ext must be 4 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read 16-bit little-endian
 */
static inline uint16_t uft_zx_sna_le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/**
 * @brief Get interrupt mode name
 */
static inline const char* uft_zx_sna_int_mode_name(uint8_t mode) {
    switch (mode) {
        case 0:  return "IM 0";
        case 1:  return "IM 1";
        case 2:  return "IM 2";
        default: return "Unknown";
    }
}

/**
 * @brief Get border color name
 */
static inline const char* uft_zx_sna_border_name(uint8_t color) {
    static const char* names[] = {
        "Black", "Blue", "Red", "Magenta",
        "Green", "Cyan", "Yellow", "White"
    };
    if (color < 8) return names[color];
    if (color == UFT_ZX_SNA_ROM_PAGED_SPEC) return "Spectrum ROM";
    if (color == UFT_ZX_SNA_ROM_PAGED_INT1) return "Interface 1 ROM";
    return "Unknown";
}

/**
 * @brief Probe for ZX SNA format
 * @return Confidence score (0-100)
 */
static inline int uft_zx_sna_probe(const uint8_t* data, size_t size) {
    if (!data) return 0;
    
    int score = 0;
    
    /* Check file size */
    if (size == UFT_ZX_SNA_SIZE_48K) {
        score += 30;
    } else if (size == UFT_ZX_SNA_SIZE_128K_SHORT || size == UFT_ZX_SNA_SIZE_128K_LONG) {
        score += 30;
    } else {
        return 0;  /* Invalid size */
    }
    
    /* Check IFF2 field (only bit 2 should be set or clear) */
    uint8_t iff2 = data[UFT_ZX_SNA_OFF_IFF2];
    if ((iff2 & ~0x04) == 0) {
        score += 25;
    } else {
        return 0;  /* Invalid IFF2 */
    }
    
    /* Check interrupt mode (0, 1, or 2) */
    uint8_t int_mode = data[UFT_ZX_SNA_OFF_INT_MODE];
    if (int_mode <= 2) {
        score += 20;
    } else {
        return 0;  /* Invalid interrupt mode */
    }
    
    /* Check border color */
    uint8_t border = data[UFT_ZX_SNA_OFF_BORDER];
    if (border <= 7 || border == UFT_ZX_SNA_ROM_PAGED_SPEC || 
        border == UFT_ZX_SNA_ROM_PAGED_INT1) {
        score += 15;
    }
    
    /* Check SP is not in ROM (warning, not rejection) */
    uint16_t sp = uft_zx_sna_le16(data + UFT_ZX_SNA_OFF_SP);
    if (sp >= UFT_ZX_SNA_RAM_START) {
        score += 10;
    }
    
    return (score > 100) ? 100 : score;
}

/**
 * @brief Parse ZX SNA header
 */
static inline bool uft_zx_sna_parse_header(const uint8_t* data, size_t size,
                                            uft_zx_sna_file_info_t* info) {
    if (!data || !info) return false;
    
    memset(info, 0, sizeof(*info));
    info->file_size = size;
    
    /* Determine type */
    if (size == UFT_ZX_SNA_SIZE_48K) {
        info->is_48k = true;
    } else if (size == UFT_ZX_SNA_SIZE_128K_SHORT || size == UFT_ZX_SNA_SIZE_128K_LONG) {
        info->is_128k = true;
    } else {
        return false;
    }
    
    /* Basic validation */
    if (uft_zx_sna_probe(data, size) < 50) {
        return false;
    }
    
    /* Extract header info */
    const uft_zx_sna_header_t* hdr = (const uft_zx_sna_header_t*)data;
    info->sp = hdr->sp;
    info->int_mode = hdr->int_mode;
    info->border = hdr->border;
    info->iff2 = (hdr->iff2 & 0x04) != 0;
    
    /* Get PC */
    if (info->is_48k) {
        /* PC is on stack - read from RAM dump */
        if (info->sp >= UFT_ZX_SNA_RAM_START) {
            size_t sp_offset = UFT_ZX_SNA_HEADER_SIZE + (info->sp - UFT_ZX_SNA_RAM_START);
            if (sp_offset + 2 <= size) {
                info->pc = uft_zx_sna_le16(data + sp_offset);
            }
        }
    } else {
        /* PC is in 128K extension */
        size_t ext_offset = UFT_ZX_SNA_HEADER_SIZE + 48 * 1024;
        const uft_zx_sna_128k_ext_t* ext = (const uft_zx_sna_128k_ext_t*)(data + ext_offset);
        info->pc = ext->pc;
        info->current_bank = ext->port_7ffd & 0x07;
    }
    
    info->valid = true;
    return true;
}

/**
 * @brief Get Z80 registers from header
 */
static inline void uft_zx_sna_get_registers(const uint8_t* data,
                                             uint16_t* af, uint16_t* bc, uint16_t* de,
                                             uint16_t* hl, uint16_t* ix, uint16_t* iy,
                                             uint16_t* sp, uint8_t* i, uint8_t* r) {
    if (!data) return;
    
    const uft_zx_sna_header_t* hdr = (const uft_zx_sna_header_t*)data;
    
    if (af) *af = hdr->af;
    if (bc) *bc = hdr->bc;
    if (de) *de = hdr->de;
    if (hl) *hl = hdr->hl;
    if (ix) *ix = hdr->ix;
    if (iy) *iy = hdr->iy;
    if (sp) *sp = hdr->sp;
    if (i) *i = hdr->i;
    if (r) *r = hdr->r;
}

/**
 * @brief Get alternate Z80 registers from header
 */
static inline void uft_zx_sna_get_alt_registers(const uint8_t* data,
                                                 uint16_t* af_alt, uint16_t* bc_alt,
                                                 uint16_t* de_alt, uint16_t* hl_alt) {
    if (!data) return;
    
    const uft_zx_sna_header_t* hdr = (const uft_zx_sna_header_t*)data;
    
    if (af_alt) *af_alt = hdr->af_alt;
    if (bc_alt) *bc_alt = hdr->bc_alt;
    if (de_alt) *de_alt = hdr->de_alt;
    if (hl_alt) *hl_alt = hdr->hl_alt;
}

/**
 * @brief Print ZX SNA file info
 */
static inline void uft_zx_sna_print_info(const uft_zx_sna_file_info_t* info) {
    if (!info) return;
    
    printf("ZX Spectrum SNA Snapshot:\n");
    printf("  Type:          %s\n", info->is_48k ? "48K" : "128K");
    printf("  File Size:     %u bytes\n", info->file_size);
    printf("  PC:            0x%04X\n", info->pc);
    printf("  SP:            0x%04X\n", info->sp);
    printf("  Int Mode:      %s\n", uft_zx_sna_int_mode_name(info->int_mode));
    printf("  Border:        %s\n", uft_zx_sna_border_name(info->border));
    printf("  IFF2:          %s\n", info->iff2 ? "Enabled" : "Disabled");
    
    if (info->is_128k) {
        printf("  Current Bank:  %d\n", info->current_bank);
    }
}

/**
 * @brief Print Z80 registers
 */
static inline void uft_zx_sna_print_registers(const uint8_t* data) {
    if (!data) return;
    
    const uft_zx_sna_header_t* hdr = (const uft_zx_sna_header_t*)data;
    
    printf("Z80 Registers:\n");
    printf("  AF=%04X  BC=%04X  DE=%04X  HL=%04X\n",
           hdr->af, hdr->bc, hdr->de, hdr->hl);
    printf("  AF'=%04X BC'=%04X DE'=%04X HL'=%04X\n",
           hdr->af_alt, hdr->bc_alt, hdr->de_alt, hdr->hl_alt);
    printf("  IX=%04X  IY=%04X  SP=%04X\n",
           hdr->ix, hdr->iy, hdr->sp);
    printf("  I=%02X  R=%02X\n", hdr->i, hdr->r);
}

/**
 * @brief Get pointer to RAM data
 */
static inline const uint8_t* uft_zx_sna_get_ram(const uint8_t* data, size_t size) {
    if (!data || size < UFT_ZX_SNA_SIZE_48K) return NULL;
    return data + UFT_ZX_SNA_HEADER_SIZE;
}

/**
 * @brief Get pointer to display memory
 */
static inline const uint8_t* uft_zx_sna_get_display(const uint8_t* data, size_t size) {
    if (!data || size < UFT_ZX_SNA_SIZE_48K) return NULL;
    return data + UFT_ZX_SNA_HEADER_SIZE;  /* Display is at start of RAM dump */
}

/**
 * @brief Get pointer to attribute memory
 */
static inline const uint8_t* uft_zx_sna_get_attributes(const uint8_t* data, size_t size) {
    if (!data || size < UFT_ZX_SNA_SIZE_48K) return NULL;
    return data + UFT_ZX_SNA_HEADER_SIZE + UFT_ZX_SNA_DISPLAY_SIZE;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_ZX_SNA_H */
