/**
 * @file uft_format_registry.h
 * @brief Unified Format Registry with Auto-Detection
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * Central registry for all 25 supported disk image formats.
 * Provides score-based auto-detection and format information.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_FORMAT_REGISTRY_H
#define UFT_FORMAT_REGISTRY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Format Registry Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief Total number of registered formats */
#define UFT_FORMAT_COUNT            26

/** @brief Minimum probe score to consider a match */
#define UFT_FORMAT_MIN_SCORE        30

/** @brief High confidence threshold */
#define UFT_FORMAT_HIGH_CONFIDENCE  80

/** @brief Maximum formats to return in detection results */
#define UFT_FORMAT_MAX_MATCHES      5

/* ═══════════════════════════════════════════════════════════════════════════
 * Format Identifiers
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Format type enumeration
 */
typedef enum {
    UFT_FORMAT_UNKNOWN = 0,
    
    /* Core formats */
    UFT_FORMAT_HFE,             /**< HxC Floppy Emulator */
    UFT_FORMAT_WOZ,             /**< Apple II WOZ */
    UFT_FORMAT_DC42,            /**< Apple DiskCopy 4.2 */
    UFT_FORMAT_D88,             /**< NEC PC-88/PC-98 */
    UFT_FORMAT_D77,             /**< Fujitsu FM-7/FM-77 */
    
    /* P1 formats */
    UFT_FORMAT_IMD,             /**< ImageDisk */
    UFT_FORMAT_TD0,             /**< Teledisk */
    UFT_FORMAT_SCP,             /**< SuperCard Pro */
    UFT_FORMAT_G64,             /**< Commodore 64 GCR */
    UFT_FORMAT_ADF,             /**< Amiga Disk File */
    
    /* P2 formats */
    UFT_FORMAT_EDSK,            /**< Extended DSK (Amstrad) */
    UFT_FORMAT_STX,             /**< Pasti (Atari ST) */
    UFT_FORMAT_IPF,             /**< SPS/CAPS (Amiga) */
    UFT_FORMAT_A2R,             /**< Applesauce (Apple II) */
    UFT_FORMAT_NIB,             /**< Apple II Nibble */
    
    /* P3 formats */
    UFT_FORMAT_FDI,             /**< Formatted Disk Image */
    UFT_FORMAT_DIM,             /**< Japanese PC */
    UFT_FORMAT_ATR,             /**< Atari 8-bit */
    UFT_FORMAT_TRD,             /**< TR-DOS (ZX Spectrum) */
    UFT_FORMAT_MSX,             /**< MSX DSK */
    UFT_FORMAT_86F,             /**< 86Box */
    UFT_FORMAT_KFX,             /**< KryoFlux RAW */
    UFT_FORMAT_MFI,             /**< MAME Floppy Image */
    UFT_FORMAT_DSK,             /**< CP/M / Apple II */
    UFT_FORMAT_ST,              /**< Atari ST Raw */
    UFT_FORMAT_KC85,            /**< KC85/Z1013 (DDR) */
    
    UFT_FORMAT_MAX
} uft_format_type_t;

/**
 * @brief Format category
 */
typedef enum {
    UFT_CAT_SECTOR,             /**< Sector-level image */
    UFT_CAT_FLUX,               /**< Flux-level image */
    UFT_CAT_BITSTREAM,          /**< Bitstream/MFM/GCR */
    UFT_CAT_RAW                 /**< Raw sector dump */
} uft_format_category_t;

/**
 * @brief Platform/system association
 */
#ifndef UFT_PLATFORM_DEFINED
#define UFT_PLATFORM_DEFINED
typedef enum {
    UFT_PLATFORM_GENERIC = 0,
    UFT_PLATFORM_AMIGA,
    UFT_PLATFORM_APPLE_II,
    UFT_PLATFORM_APPLE_MAC,
    UFT_PLATFORM_ATARI_8BIT,
    UFT_PLATFORM_ATARI_ST,
    UFT_PLATFORM_COMMODORE,
    UFT_PLATFORM_CPM,
    UFT_PLATFORM_IBM_PC,
    UFT_PLATFORM_MSX,
    UFT_PLATFORM_NEC_PC98,
    UFT_PLATFORM_FUJITSU_FM,
    UFT_PLATFORM_ZX_SPECTRUM,
    UFT_PLATFORM_DDR             /**< East German (DDR) computers */
} uft_platform_t;
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Format Information Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Format descriptor
 */
typedef struct {
    uft_format_type_t type;
    const char* name;               /**< Short name (e.g., "ADF") */
    const char* description;        /**< Full description */
    const char* extensions;         /**< File extensions (comma-separated) */
    uft_format_category_t category;
    uft_platform_t platform;
    bool supports_write;
    bool supports_convert;
    uint32_t min_file_size;
    uint32_t max_file_size;         /**< 0 = unlimited */
} uft_format_descriptor_t;

/**
 * @brief Detection result
 */
typedef struct {
    uft_format_type_t type;
    int score;                      /**< 0-100 confidence score */
    const uft_format_descriptor_t* descriptor;
} uft_format_match_t;

/**
 * @brief Detection results (multiple matches)
 */
typedef struct {
    uft_format_match_t matches[UFT_FORMAT_MAX_MATCHES];
    int count;
    uft_format_type_t best_match;
    int best_score;
} uft_format_detection_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Format Registry Table
 * ═══════════════════════════════════════════════════════════════════════════ */

static const uft_format_descriptor_t UFT_FORMAT_REGISTRY[] = {
    /* Core formats */
    { UFT_FORMAT_HFE,  "HFE",  "HxC Floppy Emulator",      "hfe",       UFT_CAT_BITSTREAM, UFT_PLATFORM_GENERIC,    true,  true,  512,    0 },
    { UFT_FORMAT_WOZ,  "WOZ",  "Apple II WOZ",             "woz",       UFT_CAT_FLUX,      UFT_PLATFORM_APPLE_II,   true,  true,  256,    0 },
    { UFT_FORMAT_DC42, "DC42", "Apple DiskCopy 4.2",       "dc42,image",UFT_CAT_SECTOR,    UFT_PLATFORM_APPLE_MAC,  true,  true,  84,     0 },
    { UFT_FORMAT_D88,  "D88",  "NEC PC-88/PC-98",          "d88,d98",   UFT_CAT_SECTOR,    UFT_PLATFORM_NEC_PC98,   true,  true,  688,    0 },
    { UFT_FORMAT_D77,  "D77",  "Fujitsu FM-7/FM-77",       "d77",       UFT_CAT_SECTOR,    UFT_PLATFORM_FUJITSU_FM, true,  true,  688,    0 },
    
    /* P1 formats */
    { UFT_FORMAT_IMD,  "IMD",  "ImageDisk",                "imd",       UFT_CAT_SECTOR,    UFT_PLATFORM_IBM_PC,     true,  true,  128,    0 },
    { UFT_FORMAT_TD0,  "TD0",  "Teledisk",                 "td0",       UFT_CAT_SECTOR,    UFT_PLATFORM_IBM_PC,     false, true,  12,     0 },
    { UFT_FORMAT_SCP,  "SCP",  "SuperCard Pro",            "scp",       UFT_CAT_FLUX,      UFT_PLATFORM_GENERIC,    true,  true,  16,     0 },
    { UFT_FORMAT_G64,  "G64",  "Commodore 64 GCR",         "g64",       UFT_CAT_BITSTREAM, UFT_PLATFORM_COMMODORE,  true,  true,  8,      0 },
    { UFT_FORMAT_ADF,  "ADF",  "Amiga Disk File",          "adf",       UFT_CAT_SECTOR,    UFT_PLATFORM_AMIGA,      true,  true,  901120, 1802240 },
    
    /* P2 formats */
    { UFT_FORMAT_EDSK, "EDSK", "Extended DSK (Amstrad)",   "dsk,edsk",  UFT_CAT_SECTOR,    UFT_PLATFORM_CPM,        true,  true,  256,    0 },
    { UFT_FORMAT_STX,  "STX",  "Pasti (Atari ST)",         "stx",       UFT_CAT_FLUX,      UFT_PLATFORM_ATARI_ST,   false, true,  16,     0 },
    { UFT_FORMAT_IPF,  "IPF",  "SPS/CAPS Interchangeable", "ipf",       UFT_CAT_FLUX,      UFT_PLATFORM_AMIGA,      false, true,  12,     0 },
    { UFT_FORMAT_A2R,  "A2R",  "Applesauce (Apple II)",    "a2r",       UFT_CAT_FLUX,      UFT_PLATFORM_APPLE_II,   true,  true,  8,      0 },
    { UFT_FORMAT_NIB,  "NIB",  "Apple II Nibble",          "nib",       UFT_CAT_BITSTREAM, UFT_PLATFORM_APPLE_II,   true,  true,  232960, 232960 },
    
    /* P3 formats */
    { UFT_FORMAT_FDI,  "FDI",  "Formatted Disk Image",     "fdi",       UFT_CAT_SECTOR,    UFT_PLATFORM_GENERIC,    true,  true,  14,     0 },
    { UFT_FORMAT_DIM,  "DIM",  "Japanese PC DIM",          "dim",       UFT_CAT_SECTOR,    UFT_PLATFORM_NEC_PC98,   true,  true,  256,    0 },
    { UFT_FORMAT_ATR,  "ATR",  "Atari 8-bit",              "atr",       UFT_CAT_SECTOR,    UFT_PLATFORM_ATARI_8BIT, true,  true,  16,     0 },
    { UFT_FORMAT_TRD,  "TRD",  "TR-DOS (ZX Spectrum)",     "trd",       UFT_CAT_SECTOR,    UFT_PLATFORM_ZX_SPECTRUM,true,  true,  655360, 655360 },
    { UFT_FORMAT_MSX,  "MSX",  "MSX Disk",                 "dsk",       UFT_CAT_RAW,       UFT_PLATFORM_MSX,        true,  true,  368640, 737280 },
    { UFT_FORMAT_86F,  "86F",  "86Box Floppy",             "86f",       UFT_CAT_FLUX,      UFT_PLATFORM_IBM_PC,     true,  true,  8,      0 },
    { UFT_FORMAT_KFX,  "KFX",  "KryoFlux RAW",             "raw",       UFT_CAT_FLUX,      UFT_PLATFORM_GENERIC,    false, true,  16,     0 },
    { UFT_FORMAT_MFI,  "MFI",  "MAME Floppy Image",        "mfi",       UFT_CAT_FLUX,      UFT_PLATFORM_GENERIC,    false, true,  16,     0 },
    { UFT_FORMAT_DSK,  "DSK",  "CP/M / Apple II DSK",      "dsk,do,po", UFT_CAT_RAW,       UFT_PLATFORM_CPM,        true,  true,  1024,   0 },
    { UFT_FORMAT_ST,   "ST",   "Atari ST Raw",             "st",        UFT_CAT_RAW,       UFT_PLATFORM_ATARI_ST,   true,  true,  368640, 1474560 },
    { UFT_FORMAT_KC85, "KC85", "KC85/Z1013 (DDR)",         "kc,kcd",    UFT_CAT_SECTOR,    UFT_PLATFORM_DDR,        true,  true,  160*1024, 1000*1024 },
    
    { UFT_FORMAT_UNKNOWN, NULL, NULL, NULL, 0, 0, false, false, 0, 0 }
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Include All Format Headers for Probe Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

#include "uft_hfe_format.h"
#include "uft_woz_format.h"
#include "uft_dc42_format.h"
#include "uft_d88_format.h"
#include "uft_d77_format.h"
#include "uft_imd_format.h"
#include "uft_td0_format.h"
#include "uft_scp_format.h"
#include "uft_g64_format.h"
#include "uft_adf_format.h"
#include "uft_edsk_format.h"
#include "uft_stx_format.h"
#include "uft_ipf_format.h"
#include "uft_a2r_format.h"
#include "uft_nib_format.h"
#include "uft_fdi_format.h"
#include "uft_dim_format.h"
#include "uft_atr_format.h"
#include "uft_trd_format.h"
#include "uft_msx_format.h"
#include "uft_86f_format.h"
#include "uft_kfx_format.h"
#include "uft_mfi_format.h"
#include "uft_dsk_format.h"
#include "uft_st_format.h"
#include "uft_kc85_format.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * Registry Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get format descriptor by type
 */
static inline const uft_format_descriptor_t* uft_format_get_descriptor(uft_format_type_t type) {
    if (type <= UFT_FORMAT_UNKNOWN || type >= UFT_FORMAT_MAX) return NULL;
    return &UFT_FORMAT_REGISTRY[type - 1];
}

/**
 * @brief Get format name
 */
static inline const char* uft_format_get_name(uft_format_type_t type) {
    const uft_format_descriptor_t* desc = uft_format_get_descriptor(type);
    return desc ? desc->name : "UNKNOWN";
}

/**
 * @brief Get format description
 */
static inline const char* uft_format_get_description(uft_format_type_t type) {
    const uft_format_descriptor_t* desc = uft_format_get_descriptor(type);
    return desc ? desc->description : "Unknown format";
}

/**
 * @brief Get category name
 */
static inline const char* uft_format_category_name(uft_format_category_t cat) {
    switch (cat) {
        case UFT_CAT_SECTOR:    return "Sector";
        case UFT_CAT_FLUX:      return "Flux";
        case UFT_CAT_BITSTREAM: return "Bitstream";
        case UFT_CAT_RAW:       return "Raw";
        default: return "Unknown";
    }
}

/**
 * @brief Get platform name
 */
static inline const char* uft_format_platform_name(uft_platform_t plat) {
    switch (plat) {
        case UFT_PLATFORM_AMIGA:       return "Amiga";
        case UFT_PLATFORM_APPLE_II:    return "Apple II";
        case UFT_PLATFORM_APPLE_MAC:   return "Macintosh";
        case UFT_PLATFORM_ATARI_8BIT:  return "Atari 8-bit";
        case UFT_PLATFORM_ATARI_ST:    return "Atari ST";
        case UFT_PLATFORM_COMMODORE:   return "Commodore";
        case UFT_PLATFORM_CPM:         return "CP/M";
        case UFT_PLATFORM_IBM_PC:      return "IBM PC";
        case UFT_PLATFORM_MSX:         return "MSX";
        case UFT_PLATFORM_NEC_PC98:    return "NEC PC-98";
        case UFT_PLATFORM_FUJITSU_FM:  return "Fujitsu FM";
        case UFT_PLATFORM_ZX_SPECTRUM: return "ZX Spectrum";
        case UFT_PLATFORM_DDR:         return "DDR (East German)";
        default: return "Generic";
    }
}

/**
 * @brief Probe single format
 */
static inline int uft_format_probe_single(uft_format_type_t type, const uint8_t* data, size_t size) {
    switch (type) {
        case UFT_FORMAT_HFE:  return uft_hfe_probe(data, size);
        case UFT_FORMAT_WOZ:  return uft_woz_probe(data, size);
        case UFT_FORMAT_DC42: return uft_dc42_probe(data, size);
        case UFT_FORMAT_D88:  return uft_d88_probe(data, size);
        case UFT_FORMAT_D77:  return uft_d77_probe(data, size);
        case UFT_FORMAT_IMD:  return uft_imd_probe(data, size);
        case UFT_FORMAT_TD0:  return uft_td0_probe(data, size);
        case UFT_FORMAT_SCP:  return uft_scp_probe(data, size);
        case UFT_FORMAT_G64:  return uft_g64_probe(data, size);
        case UFT_FORMAT_ADF:  return uft_adf_probe(data, size);
        case UFT_FORMAT_EDSK: return uft_edsk_probe(data, size);
        case UFT_FORMAT_STX:  return uft_stx_probe(data, size);
        case UFT_FORMAT_IPF:  return uft_ipf_probe(data, size);
        case UFT_FORMAT_A2R:  return uft_a2r_probe(data, size);
        case UFT_FORMAT_NIB:  return uft_nib_probe(data, size);
        case UFT_FORMAT_FDI:  return uft_fdi_probe(data, size);
        case UFT_FORMAT_DIM:  return uft_dim_probe(data, size);
        case UFT_FORMAT_ATR:  return uft_atr_probe(data, size);
        case UFT_FORMAT_TRD:  return uft_trd_probe(data, size);
        case UFT_FORMAT_MSX:  return uft_msx_probe(data, size);
        case UFT_FORMAT_86F:  return uft_86f_probe(data, size);
        case UFT_FORMAT_KFX:  return uft_kfx_probe(data, size);
        case UFT_FORMAT_MFI:  return uft_mfi_probe(data, size);
        case UFT_FORMAT_DSK:  return uft_dsk_probe(data, size);
        case UFT_FORMAT_ST:   return uft_st_probe(data, size);
        case UFT_FORMAT_KC85: return uft_kc85_probe(data, size);
        default: return 0;
    }
}

/**
 * @brief Auto-detect format from data
 * @param data File data
 * @param size Data size
 * @param result Detection results (output)
 * @return Best matching format type, or UFT_FORMAT_UNKNOWN
 */
static inline uft_format_type_t uft_format_detect(const uint8_t* data, size_t size, 
                                                   uft_format_detection_t* result) {
    if (!data || size == 0) {
        if (result) {
            result->count = 0;
            result->best_match = UFT_FORMAT_UNKNOWN;
            result->best_score = 0;
        }
        return UFT_FORMAT_UNKNOWN;
    }
    
    /* Temporary storage for all scores */
    struct { uft_format_type_t type; int score; } scores[UFT_FORMAT_MAX];
    int score_count = 0;
    
    /* Probe all formats */
    for (uft_format_type_t t = UFT_FORMAT_HFE; t < UFT_FORMAT_MAX; t++) {
        int score = uft_format_probe_single(t, data, size);
        if (score >= UFT_FORMAT_MIN_SCORE) {
            scores[score_count].type = t;
            scores[score_count].score = score;
            score_count++;
        }
    }
    
    /* Sort by score (descending) - simple bubble sort for small array */
    for (int i = 0; i < score_count - 1; i++) {
        for (int j = i + 1; j < score_count; j++) {
            if (scores[j].score > scores[i].score) {
                uft_format_type_t tmp_t = scores[i].type;
                int tmp_s = scores[i].score;
                scores[i].type = scores[j].type;
                scores[i].score = scores[j].score;
                scores[j].type = tmp_t;
                scores[j].score = tmp_s;
            }
        }
    }
    
    /* Fill result structure */
    if (result) {
        result->count = (score_count < UFT_FORMAT_MAX_MATCHES) ? score_count : UFT_FORMAT_MAX_MATCHES;
        for (int i = 0; i < result->count; i++) {
            result->matches[i].type = scores[i].type;
            result->matches[i].score = scores[i].score;
            result->matches[i].descriptor = uft_format_get_descriptor(scores[i].type);
        }
        result->best_match = (score_count > 0) ? scores[0].type : UFT_FORMAT_UNKNOWN;
        result->best_score = (score_count > 0) ? scores[0].score : 0;
    }
    
    return (score_count > 0) ? scores[0].type : UFT_FORMAT_UNKNOWN;
}

/**
 * @brief Simple auto-detect (returns best match only)
 */
static inline uft_format_type_t uft_format_identify(const uint8_t* data, size_t size) {
    return uft_format_detect(data, size, NULL);
}

/**
 * @brief Check if format supports writing
 */
static inline bool uft_format_can_write(uft_format_type_t type) {
    const uft_format_descriptor_t* desc = uft_format_get_descriptor(type);
    return desc ? desc->supports_write : false;
}

/**
 * @brief Check if format supports conversion
 */
static inline bool uft_format_can_convert(uft_format_type_t type) {
    const uft_format_descriptor_t* desc = uft_format_get_descriptor(type);
    return desc ? desc->supports_convert : false;
}

/**
 * @brief Get all formats for a platform
 */
static inline int uft_format_get_by_platform(uft_platform_t platform, 
                                              uft_format_type_t* types, int max_types) {
    int count = 0;
    for (int i = 0; UFT_FORMAT_REGISTRY[i].name != NULL && count < max_types; i++) {
        if (UFT_FORMAT_REGISTRY[i].platform == platform) {
            types[count++] = UFT_FORMAT_REGISTRY[i].type;
        }
    }
    return count;
}

/**
 * @brief Print detection results (debug helper)
 */
static inline void uft_format_print_detection(const uft_format_detection_t* result) {
    if (!result || result->count == 0) {
        printf("No format detected\n");
        return;
    }
    
    printf("Detected %d possible format(s):\n", result->count);
    for (int i = 0; i < result->count; i++) {
        const uft_format_match_t* m = &result->matches[i];
        printf("  %d. %s (%s) - Score: %d%%\n", 
               i + 1,
               m->descriptor->name,
               m->descriptor->description,
               m->score);
    }
    printf("Best match: %s (%d%% confidence)\n",
           uft_format_get_name(result->best_match),
           result->best_score);
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMAT_REGISTRY_H */
