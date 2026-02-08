#ifndef UFT_SPECTRUM_NEX_H
#define UFT_SPECTRUM_NEX_H

/**
 * @file uft_spectrum_nex.h
 * @brief ZX Spectrum Next NEX Executable Format
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * NEX is the native executable format for ZX Spectrum Next.
 * Modern format supporting all Next hardware features.
 *
 * File Structure:
 * - 512-byte header
 * - Optional loading screen(s)
 * - Banked data
 *
 * Header (512 bytes):
 * - Magic: "Next" (4 bytes)
 * - Version string: "V1.x" (4 bytes)
 * - RAM required: Minimum RAM (1 byte)
 * - Banks required: Number of 16K banks (1 byte)
 * - Loading screens: 0=none, 1=layer2, 2=ULA, 3=LoRes, etc.
 * - Border color: 0-7
 * - Stack pointer: SP value
 * - Program counter: Entry point
 * - Number of extra banks
 * - Bank data offsets
 *
 * Memory Banking:
 * - 16K banks (0-223)
 * - 8K pages (0-447)
 * - Bank N maps 2 pages (N*2, N*2+1)
 *
 * Features:
 * - Layer 2 graphics support
 * - Copper coprocessor
 * - DMA
 * - Enhanced ULA
 * - Tilemap
 *
 * References:
 * - ZX Spectrum Next User Manual
 * - NextZXOS documentation
 * - RetroGhidra SpectrumNextNexLoader.java (stub)
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
 * NEX Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief NEX magic signature */
#define UFT_NEX_MAGIC               "Next"
#define UFT_NEX_MAGIC_LEN           4

/** @brief NEX header size */
#define UFT_NEX_HEADER_SIZE         512

/** @brief NEX version strings */
#define UFT_NEX_VERSION_V1_0        "V1.0"
#define UFT_NEX_VERSION_V1_1        "V1.1"
#define UFT_NEX_VERSION_V1_2        "V1.2"
#define UFT_NEX_VERSION_V1_3        "V1.3"

/** @brief Bank/page sizes */
#define UFT_NEX_BANK_SIZE           16384   /**< 16K per bank */
#define UFT_NEX_PAGE_SIZE           8192    /**< 8K per page */
#define UFT_NEX_MAX_BANKS           224     /**< Banks 0-223 */

/** @brief Loading screen types */
#define UFT_NEX_SCREEN_NONE         0       /**< No loading screen */
#define UFT_NEX_SCREEN_LAYER2       1       /**< Layer 2 256x192x8 */
#define UFT_NEX_SCREEN_ULA          2       /**< Standard ULA screen */
#define UFT_NEX_SCREEN_LORES        3       /**< LoRes 128x96x8 */
#define UFT_NEX_SCREEN_HIRES        4       /**< HiRes 512x192x2 */
#define UFT_NEX_SCREEN_HICOL        5       /**< HiColour 256x192x8 */
#define UFT_NEX_SCREEN_LAYER2_320   6       /**< Layer 2 320x256x8 */
#define UFT_NEX_SCREEN_LAYER2_640   7       /**< Layer 2 640x256x4 */

/** @brief Header offsets */
#define UFT_NEX_OFF_MAGIC           0x00    /**< "Next" */
#define UFT_NEX_OFF_VERSION         0x04    /**< "V1.x" */
#define UFT_NEX_OFF_RAM_REQUIRED    0x08    /**< RAM required (MB) */
#define UFT_NEX_OFF_NUM_BANKS       0x09    /**< Number of 16K banks */
#define UFT_NEX_OFF_LOADING_SCREEN  0x0A    /**< Loading screen type */
#define UFT_NEX_OFF_BORDER_COLOUR   0x0B    /**< Border colour */
#define UFT_NEX_OFF_SP              0x0C    /**< Stack pointer (LE) */
#define UFT_NEX_OFF_PC              0x0E    /**< Program counter (LE) */
#define UFT_NEX_OFF_NUM_EXTRA       0x10    /**< Number of extra files */
#define UFT_NEX_OFF_BANK_ORDER      0x12    /**< Bank order bitmap */
#define UFT_NEX_OFF_LOAD_BAR        0x82    /**< Loading bar colour */
#define UFT_NEX_OFF_LOAD_BAR_Y      0x83    /**< Loading bar Y position */
#define UFT_NEX_OFF_LOAD_DELAY      0x84    /**< Loading delay */
#define UFT_NEX_OFF_START_DELAY     0x85    /**< Start delay */
#define UFT_NEX_OFF_PRESERVE_REGS   0x86    /**< Preserve current NextReg */
#define UFT_NEX_OFF_REQUIRED_CORE   0x87    /**< Required core version */
#define UFT_NEX_OFF_FILE_HANDLE     0x8A    /**< File handle action */
#define UFT_NEX_OFF_ENTRY_BANK      0x8B    /**< Entry bank */
#define UFT_NEX_OFF_LAYER2_OFFSET   0x8C    /**< Layer 2 load address */

/* ═══════════════════════════════════════════════════════════════════════════
 * NEX Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief NEX header (512 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    char magic[4];              /**< "Next" */
    char version[4];            /**< "V1.x" */
    uint8_t ram_required;       /**< RAM required (0=768K, 1=1792K, 2=256K, etc) */
    uint8_t num_banks;          /**< Number of 16K banks to load */
    uint8_t loading_screen;     /**< Loading screen type */
    uint8_t border_colour;      /**< Border colour (0-7) */
    uint16_t sp;                /**< Stack pointer (LE) */
    uint16_t pc;                /**< Entry point (LE) */
    uint16_t num_extra;         /**< Number of extra files (LE) */
    uint8_t bank_order[112];    /**< Bitmap: which banks to load */
    uint8_t load_bar_colour;    /**< Loading bar colour */
    uint8_t load_bar_y;         /**< Loading bar Y position */
    uint8_t load_delay;         /**< Loading delay */
    uint8_t start_delay;        /**< Start delay */
    uint8_t preserve_regs;      /**< Preserve NextRegs flag */
    uint8_t required_core[3];   /**< Required core version (major.minor.sub) */
    uint8_t file_handle;        /**< File handle action */
    uint8_t entry_bank;         /**< Entry bank */
    uint16_t layer2_load_addr;  /**< Layer 2 load address (LE) */
    uint8_t reserved[370];      /**< Reserved/padding to 512 bytes */
} uft_nex_header_t;
#pragma pack(pop)

/**
 * @brief NEX file information
 */
typedef struct {
    char version[8];            /**< Version string */
    uint8_t version_major;      /**< Parsed major version */
    uint8_t version_minor;      /**< Parsed minor version */
    uint8_t ram_required_mb;    /**< RAM in MB */
    uint8_t num_banks;
    uint8_t loading_screen;
    uint8_t border_colour;
    uint16_t sp;
    uint16_t pc;
    uint16_t num_extra_files;
    uint8_t entry_bank;
    uint8_t required_core[3];
    uint32_t file_size;
    uint32_t screen_offset;     /**< Offset to loading screen */
    uint32_t screen_size;       /**< Loading screen size */
    uint32_t banks_offset;      /**< Offset to bank data */
    uint32_t total_bank_data;   /**< Total bytes of bank data */
    int bank_count;             /**< Actual number of banks in order */
    bool valid;
} uft_nex_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_nex_header_t) == 512, "NEX header must be 512 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read 16-bit little-endian
 */
static inline uint16_t uft_nex_le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/**
 * @brief Get loading screen type name
 */
static inline const char* uft_nex_screen_name(uint8_t type) {
    switch (type) {
        case UFT_NEX_SCREEN_NONE:       return "None";
        case UFT_NEX_SCREEN_LAYER2:     return "Layer 2 (256x192)";
        case UFT_NEX_SCREEN_ULA:        return "ULA (256x192)";
        case UFT_NEX_SCREEN_LORES:      return "LoRes (128x96)";
        case UFT_NEX_SCREEN_HIRES:      return "HiRes (512x192)";
        case UFT_NEX_SCREEN_HICOL:      return "HiColour (256x192)";
        case UFT_NEX_SCREEN_LAYER2_320: return "Layer 2 (320x256)";
        case UFT_NEX_SCREEN_LAYER2_640: return "Layer 2 (640x256)";
        default:                        return "Unknown";
    }
}

/**
 * @brief Get loading screen size in bytes
 */
static inline uint32_t uft_nex_screen_size(uint8_t type) {
    switch (type) {
        case UFT_NEX_SCREEN_NONE:       return 0;
        case UFT_NEX_SCREEN_LAYER2:     return 256 * 192;      /* 49152 */
        case UFT_NEX_SCREEN_ULA:        return 6144 + 768;     /* 6912 */
        case UFT_NEX_SCREEN_LORES:      return 128 * 96;       /* 12288 */
        case UFT_NEX_SCREEN_HIRES:      return 512 * 192 / 8;  /* 12288 */
        case UFT_NEX_SCREEN_HICOL:      return 256 * 192;      /* 49152 */
        case UFT_NEX_SCREEN_LAYER2_320: return 320 * 256;      /* 81920 */
        case UFT_NEX_SCREEN_LAYER2_640: return 640 * 256 / 2;  /* 81920 */
        default:                        return 0;
    }
}

/**
 * @brief Get RAM requirement name
 */
static inline const char* uft_nex_ram_name(uint8_t code) {
    switch (code) {
        case 0:  return "768K";
        case 1:  return "1792K";
        case 2:  return "256K";
        default: return "Unknown";
    }
}

/**
 * @brief Verify NEX signature
 */
static inline bool uft_nex_verify_signature(const uint8_t* data, size_t size) {
    if (!data || size < UFT_NEX_HEADER_SIZE) return false;
    return memcmp(data, UFT_NEX_MAGIC, UFT_NEX_MAGIC_LEN) == 0;
}

/**
 * @brief Probe for NEX format
 * @return Confidence score (0-100)
 */
static inline int uft_nex_probe(const uint8_t* data, size_t size) {
    if (!data || size < UFT_NEX_HEADER_SIZE) return 0;
    
    int score = 0;
    
    /* Check magic "Next" */
    if (memcmp(data, UFT_NEX_MAGIC, UFT_NEX_MAGIC_LEN) == 0) {
        score += 50;
    } else {
        return 0;
    }
    
    /* Check version string "V1.x" */
    if (data[4] == 'V' && data[5] == '1' && data[6] == '.') {
        uint8_t minor = data[7];
        if (minor >= '0' && minor <= '9') {
            score += 25;
        }
    }
    
    /* Check border colour (0-7) */
    uint8_t border = data[UFT_NEX_OFF_BORDER_COLOUR];
    if (border <= 7) {
        score += 10;
    }
    
    /* Check loading screen type */
    uint8_t screen = data[UFT_NEX_OFF_LOADING_SCREEN];
    if (screen <= UFT_NEX_SCREEN_LAYER2_640) {
        score += 10;
    }
    
    /* Check SP is reasonable */
    uint16_t sp = uft_nex_le16(data + UFT_NEX_OFF_SP);
    if (sp >= 0x4000 && sp <= 0xFFFF) {
        score += 5;
    }
    
    return (score > 100) ? 100 : score;
}

/**
 * @brief Parse NEX header
 */
static inline bool uft_nex_parse(const uint8_t* data, size_t size,
                                  uft_nex_info_t* info) {
    if (!data || !info || size < UFT_NEX_HEADER_SIZE) return false;
    
    memset(info, 0, sizeof(*info));
    
    /* Verify signature */
    if (!uft_nex_verify_signature(data, size)) return false;
    
    const uft_nex_header_t* hdr = (const uft_nex_header_t*)data;
    
    info->file_size = size;
    
    /* Extract version */
    memcpy(info->version, hdr->version, 4);
    info->version[4] = '\0';
    info->version_major = (hdr->version[1] - '0');
    info->version_minor = (hdr->version[3] - '0');
    
    /* Basic fields */
    info->ram_required_mb = hdr->ram_required;
    info->num_banks = hdr->num_banks;
    info->loading_screen = hdr->loading_screen;
    info->border_colour = hdr->border_colour;
    info->sp = hdr->sp;
    info->pc = hdr->pc;
    info->num_extra_files = hdr->num_extra;
    info->entry_bank = hdr->entry_bank;
    
    memcpy(info->required_core, hdr->required_core, 3);
    
    /* Calculate offsets */
    info->screen_offset = UFT_NEX_HEADER_SIZE;
    info->screen_size = uft_nex_screen_size(info->loading_screen);
    info->banks_offset = info->screen_offset + info->screen_size;
    
    /* Count actual banks in bitmap */
    for (int i = 0; i < 112 && info->bank_count < info->num_banks; i++) {
        uint8_t byte = hdr->bank_order[i];
        for (int bit = 0; bit < 8 && info->bank_count < info->num_banks; bit++) {
            if (byte & (1 << bit)) {
                info->bank_count++;
            }
        }
    }
    
    info->total_bank_data = info->bank_count * UFT_NEX_BANK_SIZE;
    
    info->valid = true;
    return true;
}

/**
 * @brief Print NEX info
 */
static inline void uft_nex_print_info(const uft_nex_info_t* info) {
    if (!info) return;
    
    printf("ZX Spectrum Next NEX Executable:\n");
    printf("  Version:        %s\n", info->version);
    printf("  RAM Required:   %s\n", uft_nex_ram_name(info->ram_required_mb));
    printf("  Banks:          %d\n", info->num_banks);
    printf("  Loading Screen: %s\n", uft_nex_screen_name(info->loading_screen));
    printf("  Border Colour:  %d\n", info->border_colour);
    printf("  SP:             $%04X\n", info->sp);
    printf("  PC (Entry):     $%04X\n", info->pc);
    printf("  Entry Bank:     %d\n", info->entry_bank);
    printf("  Core Required:  %d.%d.%d\n", 
           info->required_core[0], info->required_core[1], info->required_core[2]);
    printf("  Extra Files:    %d\n", info->num_extra_files);
    printf("  File Size:      %u bytes\n", info->file_size);
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_SPECTRUM_NEX_H */
