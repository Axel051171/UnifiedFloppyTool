/**
 * @file uft_cpc_sna.h
 * @brief Amstrad CPC SNA Snapshot Format
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * SNA is a memory snapshot format for Amstrad CPC emulators.
 * Different from ZX Spectrum SNA (same extension, different format).
 *
 * File Structure:
 * - 256-byte header (0x100)
 * - 64KB or 128KB RAM dump
 * - Optional: MEM chunks (v3)
 *
 * Supported CPC Models:
 * - CPC 464 (type 0)
 * - CPC 664 (type 1)
 * - CPC 6128 (type 2)
 *
 * Header includes full Z80 state, CRTC, Gate Array, PPI,
 * PSG (AY-3-8912), and FDC registers.
 *
 * References:
 * - https://www.cpcwiki.eu/index.php/Format:SNA_snapshot_file_format
 * - CPCEMU documentation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_CPC_SNA_H
#define UFT_CPC_SNA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * CPC SNA Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief CPC SNA signature */
#define UFT_CPC_SNA_SIGNATURE       "MV - SNA"
#define UFT_CPC_SNA_SIGNATURE_LEN   8

/** @brief CPC SNA header size */
#define UFT_CPC_SNA_HEADER_SIZE     256  /* 0x100 */

/** @brief 64K snapshot size */
#define UFT_CPC_SNA_SIZE_64K        (256 + 64 * 1024)   /* 65792 */

/** @brief 128K snapshot size */
#define UFT_CPC_SNA_SIZE_128K       (256 + 128 * 1024)  /* 131328 */

/* ═══════════════════════════════════════════════════════════════════════════
 * CPC SNA Header Offsets
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_CPC_SNA_OFF_SIGNATURE   0x00    /**< "MV - SNA" */
#define UFT_CPC_SNA_OFF_UNUSED      0x08    /**< Unused (4 bytes) */
#define UFT_CPC_SNA_OFF_VERSION     0x10    /**< Snapshot version */
/* Z80 Registers */
#define UFT_CPC_SNA_OFF_F           0x11    /**< F register */
#define UFT_CPC_SNA_OFF_A           0x12    /**< A register */
#define UFT_CPC_SNA_OFF_C           0x13    /**< C register */
#define UFT_CPC_SNA_OFF_B           0x14    /**< B register */
#define UFT_CPC_SNA_OFF_E           0x15    /**< E register */
#define UFT_CPC_SNA_OFF_D           0x16    /**< D register */
#define UFT_CPC_SNA_OFF_L           0x17    /**< L register */
#define UFT_CPC_SNA_OFF_H           0x18    /**< H register */
#define UFT_CPC_SNA_OFF_R           0x19    /**< R register */
#define UFT_CPC_SNA_OFF_I           0x1A    /**< I register */
#define UFT_CPC_SNA_OFF_IFF0        0x1B    /**< IFF0 */
#define UFT_CPC_SNA_OFF_IFF1        0x1C    /**< IFF1 */
#define UFT_CPC_SNA_OFF_IX_LO       0x1D    /**< IX low */
#define UFT_CPC_SNA_OFF_IX_HI       0x1E    /**< IX high */
#define UFT_CPC_SNA_OFF_IY_LO       0x1F    /**< IY low */
#define UFT_CPC_SNA_OFF_IY_HI       0x20    /**< IY high */
#define UFT_CPC_SNA_OFF_SP          0x21    /**< SP (16-bit) */
#define UFT_CPC_SNA_OFF_PC          0x23    /**< PC (16-bit) */
#define UFT_CPC_SNA_OFF_INT_MODE    0x25    /**< Interrupt mode */
#define UFT_CPC_SNA_OFF_F_ALT       0x26    /**< F' register */
#define UFT_CPC_SNA_OFF_A_ALT       0x27    /**< A' register */
#define UFT_CPC_SNA_OFF_C_ALT       0x28    /**< C' register */
#define UFT_CPC_SNA_OFF_B_ALT       0x29    /**< B' register */
#define UFT_CPC_SNA_OFF_E_ALT       0x2A    /**< E' register */
#define UFT_CPC_SNA_OFF_D_ALT       0x2B    /**< D' register */
#define UFT_CPC_SNA_OFF_L_ALT       0x2C    /**< L' register */
#define UFT_CPC_SNA_OFF_H_ALT       0x2D    /**< H' register */
/* Gate Array */
#define UFT_CPC_SNA_OFF_GA_PEN      0x2E    /**< GA: Pen select */
#define UFT_CPC_SNA_OFF_GA_PENS     0x2F    /**< GA: Pen colours (17 bytes) */
#define UFT_CPC_SNA_OFF_GA_ROMCFG   0x40    /**< GA: ROM config + mode */
#define UFT_CPC_SNA_OFF_GA_RAMCFG   0x41    /**< GA: RAM config */
/* CRTC */
#define UFT_CPC_SNA_OFF_CRTC_SEL    0x42    /**< CRTC: Selected register */
#define UFT_CPC_SNA_OFF_CRTC_REGS   0x43    /**< CRTC: Registers (18 bytes) */
/* Other */
#define UFT_CPC_SNA_OFF_ROM_SEL     0x55    /**< Upper ROM selection */
/* PPI */
#define UFT_CPC_SNA_OFF_PPI_A       0x56    /**< PPI port A */
#define UFT_CPC_SNA_OFF_PPI_B       0x57    /**< PPI port B */
#define UFT_CPC_SNA_OFF_PPI_C       0x58    /**< PPI port C */
#define UFT_CPC_SNA_OFF_PPI_CTRL    0x59    /**< PPI control */
/* PSG (AY-3-8912) */
#define UFT_CPC_SNA_OFF_PSG_SEL     0x5A    /**< PSG: Selected register */
#define UFT_CPC_SNA_OFF_PSG_REGS    0x5B    /**< PSG: Registers (16 bytes) */
/* Memory dump size */
#define UFT_CPC_SNA_OFF_DUMP_SIZE   0x6B    /**< Memory dump size (KB) */
/* CPC Type */
#define UFT_CPC_SNA_OFF_CPC_TYPE    0x6D    /**< CPC type (0/1/2) */
/* v2+ extensions */
#define UFT_CPC_SNA_OFF_INT_NUM     0x6E    /**< Interrupt number (v2+) */
#define UFT_CPC_SNA_OFF_MULTIMODE   0x6F    /**< Multimode bytes (6) (v2+) */

/* ═══════════════════════════════════════════════════════════════════════════
 * CPC Types
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_CPC_TYPE_464            0       /**< CPC 464 */
#define UFT_CPC_TYPE_664            1       /**< CPC 664 */
#define UFT_CPC_TYPE_6128           2       /**< CPC 6128 */

/* ═══════════════════════════════════════════════════════════════════════════
 * CPC SNA Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief CPC SNA header (256 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    char signature[8];          /**< "MV - SNA" */
    uint8_t unused[8];          /**< Unused bytes */
    uint8_t version;            /**< Snapshot version */
    /* Z80 Registers */
    uint8_t f;                  /**< F register */
    uint8_t a;                  /**< A register */
    uint8_t c;                  /**< C register */
    uint8_t b;                  /**< B register */
    uint8_t e;                  /**< E register */
    uint8_t d;                  /**< D register */
    uint8_t l;                  /**< L register */
    uint8_t h;                  /**< H register */
    uint8_t r;                  /**< R register */
    uint8_t i;                  /**< I register */
    uint8_t iff0;               /**< IFF0 */
    uint8_t iff1;               /**< IFF1 */
    uint8_t ix_lo;              /**< IX low */
    uint8_t ix_hi;              /**< IX high */
    uint8_t iy_lo;              /**< IY low */
    uint8_t iy_hi;              /**< IY high */
    uint16_t sp;                /**< SP (little endian) */
    uint16_t pc;                /**< PC (little endian) */
    uint8_t int_mode;           /**< Interrupt mode */
    /* Alternate registers */
    uint8_t f_alt;              /**< F' */
    uint8_t a_alt;              /**< A' */
    uint8_t c_alt;              /**< C' */
    uint8_t b_alt;              /**< B' */
    uint8_t e_alt;              /**< E' */
    uint8_t d_alt;              /**< D' */
    uint8_t l_alt;              /**< L' */
    uint8_t h_alt;              /**< H' */
    /* Gate Array */
    uint8_t ga_pen;             /**< Selected pen */
    uint8_t ga_pens[17];        /**< Pen colours */
    uint8_t ga_romcfg;          /**< ROM config + mode */
    uint8_t ga_ramcfg;          /**< RAM config */
    /* CRTC */
    uint8_t crtc_sel;           /**< Selected register */
    uint8_t crtc_regs[18];      /**< CRTC registers */
    /* Other */
    uint8_t rom_sel;            /**< Upper ROM selection */
    /* PPI */
    uint8_t ppi_a;              /**< Port A */
    uint8_t ppi_b;              /**< Port B */
    uint8_t ppi_c;              /**< Port C */
    uint8_t ppi_ctrl;           /**< Control */
    /* PSG */
    uint8_t psg_sel;            /**< Selected register */
    uint8_t psg_regs[16];       /**< PSG registers */
    /* Memory */
    uint16_t dump_size;         /**< Dump size in KB (little endian) */
    /* CPC Type */
    uint8_t cpc_type;           /**< 0=464, 1=664, 2=6128 */
    /* v2+ */
    uint8_t int_num;            /**< Interrupt number */
    uint8_t multimode[6];       /**< Multimode bytes */
    uint8_t reserved[139];      /**< Reserved/unused (to fill 256 bytes) */
} uft_cpc_sna_header_t;
#pragma pack(pop)

/**
 * @brief CPC SNA file information
 */
typedef struct {
    uint8_t version;
    uint8_t cpc_type;
    uint16_t sp;
    uint16_t pc;
    uint8_t int_mode;
    uint16_t dump_size_kb;
    uint32_t file_size;
    bool is_64k;
    bool is_128k;
    bool valid;
} uft_cpc_sna_file_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_cpc_sna_header_t) == 256, "CPC SNA header must be 256 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read 16-bit little-endian
 */
static inline uint16_t uft_cpc_sna_le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/**
 * @brief Get CPC type name
 */
static inline const char* uft_cpc_sna_type_name(uint8_t type) {
    switch (type) {
        case UFT_CPC_TYPE_464:  return "CPC 464";
        case UFT_CPC_TYPE_664:  return "CPC 664";
        case UFT_CPC_TYPE_6128: return "CPC 6128";
        default:               return "Unknown";
    }
}

/**
 * @brief Get interrupt mode name
 */
static inline const char* uft_cpc_sna_int_mode_name(uint8_t mode) {
    switch (mode) {
        case 0:  return "IM 0";
        case 1:  return "IM 1";
        case 2:  return "IM 2";
        default: return "Unknown";
    }
}

/**
 * @brief Verify CPC SNA signature
 */
static inline bool uft_cpc_sna_verify_signature(const uint8_t* data, size_t size) {
    if (!data || size < UFT_CPC_SNA_HEADER_SIZE) return false;
    return memcmp(data, UFT_CPC_SNA_SIGNATURE, UFT_CPC_SNA_SIGNATURE_LEN) == 0;
}

/**
 * @brief Probe for CPC SNA format
 * @return Confidence score (0-100)
 */
static inline int uft_cpc_sna_probe(const uint8_t* data, size_t size) {
    if (!data || size < UFT_CPC_SNA_HEADER_SIZE) return 0;
    
    int score = 0;
    
    /* Check signature */
    if (memcmp(data, UFT_CPC_SNA_SIGNATURE, UFT_CPC_SNA_SIGNATURE_LEN) == 0) {
        score += 50;
    } else {
        return 0;
    }
    
    /* Check file size */
    if (size == UFT_CPC_SNA_SIZE_64K || size == UFT_CPC_SNA_SIZE_128K) {
        score += 20;
    } else {
        /* v3 can have variable size */
        uint8_t version = data[UFT_CPC_SNA_OFF_VERSION];
        if (version >= 3 && size > UFT_CPC_SNA_HEADER_SIZE) {
            score += 10;
        }
    }
    
    /* Check version */
    uint8_t version = data[UFT_CPC_SNA_OFF_VERSION];
    if (version >= 1 && version <= 3) {
        score += 15;
    }
    
    /* Check CPC type */
    uint8_t cpc_type = data[UFT_CPC_SNA_OFF_CPC_TYPE];
    if (cpc_type <= 2) {
        score += 10;
    }
    
    /* Check interrupt mode */
    uint8_t int_mode = data[UFT_CPC_SNA_OFF_INT_MODE];
    if (int_mode <= 2) {
        score += 5;
    }
    
    return (score > 100) ? 100 : score;
}

/**
 * @brief Parse CPC SNA header
 */
static inline bool uft_cpc_sna_parse_header(const uint8_t* data, size_t size,
                                             uft_cpc_sna_file_info_t* info) {
    if (!data || !info) return false;
    if (!uft_cpc_sna_verify_signature(data, size)) return false;
    
    memset(info, 0, sizeof(*info));
    
    const uft_cpc_sna_header_t* hdr = (const uft_cpc_sna_header_t*)data;
    
    info->version = hdr->version;
    info->cpc_type = hdr->cpc_type;
    info->sp = hdr->sp;
    info->pc = hdr->pc;
    info->int_mode = hdr->int_mode;
    info->dump_size_kb = hdr->dump_size;
    info->file_size = size;
    
    /* Determine dump type */
    if (info->dump_size_kb == 64) {
        info->is_64k = true;
    } else if (info->dump_size_kb == 128) {
        info->is_128k = true;
    }
    
    info->valid = true;
    return true;
}

/**
 * @brief Get Z80 registers from header
 */
static inline void uft_cpc_sna_get_registers(const uint8_t* data,
                                              uint16_t* af, uint16_t* bc, uint16_t* de,
                                              uint16_t* hl, uint16_t* ix, uint16_t* iy,
                                              uint16_t* sp, uint16_t* pc,
                                              uint8_t* i, uint8_t* r) {
    if (!data) return;
    
    const uft_cpc_sna_header_t* hdr = (const uft_cpc_sna_header_t*)data;
    
    if (af) *af = ((uint16_t)hdr->a << 8) | hdr->f;
    if (bc) *bc = ((uint16_t)hdr->b << 8) | hdr->c;
    if (de) *de = ((uint16_t)hdr->d << 8) | hdr->e;
    if (hl) *hl = ((uint16_t)hdr->h << 8) | hdr->l;
    if (ix) *ix = ((uint16_t)hdr->ix_hi << 8) | hdr->ix_lo;
    if (iy) *iy = ((uint16_t)hdr->iy_hi << 8) | hdr->iy_lo;
    if (sp) *sp = hdr->sp;
    if (pc) *pc = hdr->pc;
    if (i) *i = hdr->i;
    if (r) *r = hdr->r;
}

/**
 * @brief Print CPC SNA file info
 */
static inline void uft_cpc_sna_print_info(const uft_cpc_sna_file_info_t* info) {
    if (!info) return;
    
    printf("Amstrad CPC SNA Snapshot:\n");
    printf("  Version:       %d\n", info->version);
    printf("  CPC Type:      %s\n", uft_cpc_sna_type_name(info->cpc_type));
    printf("  File Size:     %u bytes\n", info->file_size);
    printf("  Memory Size:   %d KB\n", info->dump_size_kb);
    printf("  PC:            0x%04X\n", info->pc);
    printf("  SP:            0x%04X\n", info->sp);
    printf("  Int Mode:      %s\n", uft_cpc_sna_int_mode_name(info->int_mode));
}

/**
 * @brief Print Z80 registers
 */
static inline void uft_cpc_sna_print_registers(const uint8_t* data) {
    if (!data) return;
    
    const uft_cpc_sna_header_t* hdr = (const uft_cpc_sna_header_t*)data;
    
    uint16_t af = ((uint16_t)hdr->a << 8) | hdr->f;
    uint16_t bc = ((uint16_t)hdr->b << 8) | hdr->c;
    uint16_t de = ((uint16_t)hdr->d << 8) | hdr->e;
    uint16_t hl = ((uint16_t)hdr->h << 8) | hdr->l;
    uint16_t ix = ((uint16_t)hdr->ix_hi << 8) | hdr->ix_lo;
    uint16_t iy = ((uint16_t)hdr->iy_hi << 8) | hdr->iy_lo;
    
    uint16_t af_alt = ((uint16_t)hdr->a_alt << 8) | hdr->f_alt;
    uint16_t bc_alt = ((uint16_t)hdr->b_alt << 8) | hdr->c_alt;
    uint16_t de_alt = ((uint16_t)hdr->d_alt << 8) | hdr->d_alt;
    uint16_t hl_alt = ((uint16_t)hdr->h_alt << 8) | hdr->l_alt;
    
    printf("Z80 Registers:\n");
    printf("  AF=%04X  BC=%04X  DE=%04X  HL=%04X\n", af, bc, de, hl);
    printf("  AF'=%04X BC'=%04X DE'=%04X HL'=%04X\n", af_alt, bc_alt, de_alt, hl_alt);
    printf("  IX=%04X  IY=%04X  SP=%04X  PC=%04X\n", ix, iy, hdr->sp, hdr->pc);
    printf("  I=%02X  R=%02X\n", hdr->i, hdr->r);
}

/**
 * @brief Get pointer to RAM data
 */
static inline const uint8_t* uft_cpc_sna_get_ram(const uint8_t* data, size_t size) {
    if (!data || size < UFT_CPC_SNA_SIZE_64K) return NULL;
    return data + UFT_CPC_SNA_HEADER_SIZE;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_CPC_SNA_H */
