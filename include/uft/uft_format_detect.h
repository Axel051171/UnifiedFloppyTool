/**
 * @file uft_format_detect.h
 * @brief UnifiedFloppyTool - Format Auto-Detection API
 * 
 * Zentrale API für Format-Erkennung.
 * Wird von GUI und CLI verwendet.
 * 
 * @author UFT Team
 * @date 2025
 */

#ifndef UFT_FORMAT_DETECT_H
#define UFT_FORMAT_DETECT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Detection Result
// ============================================================================

/**
 * @brief Erkanntes Format
 */
typedef enum {
    UFT_DETECT_UNKNOWN = 0,
    
    // Commodore
    UFT_DETECT_D64,
    UFT_DETECT_D71,
    UFT_DETECT_D81,
    UFT_DETECT_D80,
    UFT_DETECT_D82,
    UFT_DETECT_G64,
    UFT_DETECT_G71,
    
    // Amiga
    UFT_DETECT_ADF,
    
    // PC
    UFT_DETECT_IMG,
    
    // Atari ST
    UFT_DETECT_ST,
    UFT_DETECT_MSA,
    
    // Apple
    UFT_DETECT_DSK_APPLE,
    UFT_DETECT_NIB,
    UFT_DETECT_A2R,
    
    // Flux
    UFT_DETECT_SCP,
    UFT_DETECT_HFE,
    UFT_DETECT_IPF,
    UFT_DETECT_KRYOFLUX,
    
    // Archiv
    UFT_DETECT_TD0,
    UFT_DETECT_IMD,
    UFT_DETECT_FDI,
    
} uft_detect_format_t;

/**
 * @brief Erkannte Variante
 */
typedef struct {
    uft_detect_format_t format;
    
    // Varianten-Details
    const char*     variant_name;       ///< z.B. "Extended (40 Tracks)"
    const char*     description;
    
    // Confidence (0-100)
    int             confidence;
    
    // Geometrie
    uint8_t         cylinders;
    uint8_t         heads;
    uint8_t         sectors_per_track;  ///< 0 = variabel
    uint16_t        sector_size;
    uint32_t        total_size;
    
    // Features
    bool            has_error_info;
    bool            is_extended;
    bool            is_compressed;
    bool            has_copy_protection;
    bool            is_flux_level;
    bool            is_gcr_raw;
    bool            is_mfm_raw;
    
    // Filesystem-Erkennung (optional)
    const char*     filesystem;         ///< "CBM-DOS", "OFS", "FFS", "FAT12", etc.
    const char*     volume_name;        ///< Disk-Name falls vorhanden
    
    // Warnungen
    const char*     warnings[8];
    int             warning_count;
    
} uft_detect_result_t;

// ============================================================================
// Detection Functions
// ============================================================================

/**
 * @brief Erkennt Format aus Datei
 * 
 * @param path Dateipfad
 * @param result [out] Erkennungsergebnis
 * @return true wenn Format erkannt
 */
bool uft_detect_file(const char* path, uft_detect_result_t* result);

/**
 * @brief Erkennt Format aus Speicher
 * 
 * @param data Datei-Daten
 * @param size Datei-Größe
 * @param filename Optionaler Dateiname (für Extension-Check)
 * @param result [out] Erkennungsergebnis
 * @return true wenn Format erkannt
 */
bool uft_detect_buffer(const uint8_t* data, size_t size, 
                       const char* filename,
                       uft_detect_result_t* result);

/**
 * @brief Gibt Format-Name zurück
 */
const char* uft_format_name(uft_detect_format_t format);

/**
 * @brief Gibt Standard-Extension zurück
 */
const char* uft_format_extension(uft_detect_format_t format);

/**
 * @brief Prüft ob Format Schreiben unterstützt
 */
bool uft_format_is_writable(uft_detect_format_t format);

/**
 * @brief Prüft ob Format Flux-Level ist
 */
bool uft_format_is_flux(uft_detect_format_t format);

// ============================================================================
// D64 Specific Detection
// ============================================================================

/**
 * @brief D64-Varianten-Enum
 */
typedef enum {
    D64_VARIANT_35 = 0,
    D64_VARIANT_35_ERR,
    D64_VARIANT_40,
    D64_VARIANT_40_ERR,
    D64_VARIANT_42,
    D64_VARIANT_42_ERR,
    D64_VARIANT_UNKNOWN
} uft_d64_variant_t;

/**
 * @brief Erkennt D64-Variante aus Größe
 */
uft_d64_variant_t uft_d64_detect_variant(size_t file_size);

/**
 * @brief Gibt D64-Varianten-Name zurück
 */
const char* uft_d64_variant_name(uft_d64_variant_t variant);

/**
 * @brief Berechnet Dateigröße für D64-Variante
 */
size_t uft_d64_variant_size(uft_d64_variant_t variant);

/**
 * @brief Anzahl Tracks für Variante
 */
int uft_d64_variant_tracks(uft_d64_variant_t variant);

/**
 * @brief Hat Variante Error-Info?
 */
bool uft_d64_variant_has_errors(uft_d64_variant_t variant);

// ============================================================================
// ADF Specific Detection
// ============================================================================

typedef enum {
    ADF_VARIANT_DD = 0,         ///< 880 KB
    ADF_VARIANT_HD,             ///< 1.76 MB
    ADF_VARIANT_DD_EXT,         ///< 81-84 Zylinder
    ADF_VARIANT_UNKNOWN
} uft_adf_variant_t;

typedef enum {
    ADF_FS_UNKNOWN = 0,
    ADF_FS_OFS,                 ///< Old File System
    ADF_FS_FFS,                 ///< Fast File System
    ADF_FS_OFS_INTL,            ///< OFS International
    ADF_FS_FFS_INTL,            ///< FFS International
    ADF_FS_OFS_DC,              ///< OFS Dir Cache
    ADF_FS_FFS_DC,              ///< FFS Dir Cache
} uft_adf_filesystem_t;

uft_adf_variant_t uft_adf_detect_variant(size_t file_size);
uft_adf_filesystem_t uft_adf_detect_filesystem(const uint8_t* data, size_t size);
const char* uft_adf_filesystem_name(uft_adf_filesystem_t fs);

// ============================================================================
// IMG/ST Specific Detection
// ============================================================================

typedef struct {
    const char* name;
    int         cylinders;
    int         heads;
    int         sectors;
    int         sector_size;
    size_t      file_size;
    uint32_t    data_rate;      ///< bps
    bool        is_hd;
    bool        is_special;     ///< DMF, XDF, etc.
} uft_img_geometry_t;

/**
 * @brief Erkennt IMG/ST Geometrie aus Größe
 */
const uft_img_geometry_t* uft_img_detect_geometry(size_t file_size);

/**
 * @brief Liste aller bekannten IMG-Geometrien
 */
const uft_img_geometry_t* uft_img_get_geometries(size_t* count);

// ============================================================================
// Implementation Helpers
// ============================================================================

#ifdef UFT_FORMAT_DETECT_IMPLEMENTATION

// D64 Größen
static const size_t D64_SIZES[] = {
    174848,     // 35 Track
    175531,     // 35 Track + Error
    196608,     // 40 Track
    197376,     // 40 Track + Error
    205312,     // 42 Track
    206114      // 42 Track + Error
};

static const char* D64_VARIANT_NAMES[] = {
    "Standard (35 Tracks)",
    "Standard + Error Info",
    "Extended (40 Tracks)",
    "Extended + Error Info",
    "Extended (42 Tracks)",
    "Extended (42 Tracks) + Error Info",
    "Unknown"
};

uft_d64_variant_t uft_d64_detect_variant(size_t file_size) {
    for (int i = 0; i < 6; i++) {
        if (file_size == D64_SIZES[i]) {
            return (uft_d64_variant_t)i;
        }
    }
    return D64_VARIANT_UNKNOWN;
}

const char* uft_d64_variant_name(uft_d64_variant_t variant) {
    if (variant >= 0 && variant <= D64_VARIANT_42_ERR) {
        return D64_VARIANT_NAMES[variant];
    }
    return D64_VARIANT_NAMES[D64_VARIANT_UNKNOWN];
}

size_t uft_d64_variant_size(uft_d64_variant_t variant) {
    if (variant >= 0 && variant <= D64_VARIANT_42_ERR) {
        return D64_SIZES[variant];
    }
    return 0;
}

int uft_d64_variant_tracks(uft_d64_variant_t variant) {
    switch (variant) {
        case D64_VARIANT_35:
        case D64_VARIANT_35_ERR:
            return 35;
        case D64_VARIANT_40:
        case D64_VARIANT_40_ERR:
            return 40;
        case D64_VARIANT_42:
        case D64_VARIANT_42_ERR:
            return 42;
        default:
            return 0;
    }
}

bool uft_d64_variant_has_errors(uft_d64_variant_t variant) {
    switch (variant) {
        case D64_VARIANT_35_ERR:
        case D64_VARIANT_40_ERR:
        case D64_VARIANT_42_ERR:
            return true;
        default:
            return false;
    }
}

// IMG Geometrien
static const uft_img_geometry_t IMG_GEOMETRIES[] = {
    { "160 KB 5.25\" SS/DD",  40, 1,  8, 512,  163840, 250000, false, false },
    { "180 KB 5.25\" SS/DD",  40, 1,  9, 512,  184320, 250000, false, false },
    { "320 KB 5.25\" DS/DD",  40, 2,  8, 512,  327680, 250000, false, false },
    { "360 KB 5.25\" DS/DD",  40, 2,  9, 512,  368640, 250000, false, false },
    { "720 KB 3.5\" DS/DD",   80, 2,  9, 512,  737280, 250000, false, false },
    { "1.2 MB 5.25\" DS/HD",  80, 2, 15, 512, 1228800, 500000, true,  false },
    { "1.44 MB 3.5\" DS/HD",  80, 2, 18, 512, 1474560, 500000, true,  false },
    { "2.88 MB 3.5\" DS/ED",  80, 2, 36, 512, 2949120, 1000000, true, false },
    { "1.68 MB DMF",          80, 2, 21, 512, 1720320, 500000, true,  true  },
    { "1.72 MB XDF",          80, 2, 0,  512, 1763328, 500000, true,  true  },
    { NULL, 0, 0, 0, 0, 0, 0, false, false }
};

const uft_img_geometry_t* uft_img_detect_geometry(size_t file_size) {
    for (const uft_img_geometry_t* g = IMG_GEOMETRIES; g->name; g++) {
        if (g->file_size == file_size) {
            return g;
        }
    }
    return NULL;
}

const uft_img_geometry_t* uft_img_get_geometries(size_t* count) {
    if (count) {
        *count = 0;
        for (const uft_img_geometry_t* g = IMG_GEOMETRIES; g->name; g++) {
            (*count)++;
        }
    }
    return IMG_GEOMETRIES;
}

// Format Namen
const char* uft_format_name(uft_detect_format_t format) {
    switch (format) {
        case UFT_DETECT_D64:      return "D64 (Commodore 64)";
        case UFT_DETECT_D71:      return "D71 (Commodore 1571)";
        case UFT_DETECT_D81:      return "D81 (Commodore 1581)";
        case UFT_DETECT_D80:      return "D80 (Commodore 8050)";
        case UFT_DETECT_D82:      return "D82 (Commodore 8250)";
        case UFT_DETECT_G64:      return "G64 (GCR Image)";
        case UFT_DETECT_G71:      return "G71 (GCR Double-Sided)";
        case UFT_DETECT_ADF:      return "ADF (Amiga)";
        case UFT_DETECT_IMG:      return "IMG (PC Disk Image)";
        case UFT_DETECT_ST:       return "ST (Atari ST)";
        case UFT_DETECT_MSA:      return "MSA (Atari Compressed)";
        case UFT_DETECT_DSK_APPLE: return "DSK (Apple II)";
        case UFT_DETECT_NIB:      return "NIB (Apple Nibble)";
        case UFT_DETECT_A2R:      return "A2R (Applesauce)";
        case UFT_DETECT_SCP:      return "SCP (SuperCard Pro)";
        case UFT_DETECT_HFE:      return "HFE (HxC Floppy)";
        case UFT_DETECT_IPF:      return "IPF (CAPS)";
        case UFT_DETECT_KRYOFLUX: return "KryoFlux Stream";
        case UFT_DETECT_TD0:      return "TD0 (Teledisk)";
        case UFT_DETECT_IMD:      return "IMD (ImageDisk)";
        case UFT_DETECT_FDI:      return "FDI";
        default:                  return "Unknown";
    }
}

const char* uft_format_extension(uft_detect_format_t format) {
    switch (format) {
        case UFT_DETECT_D64:      return "d64";
        case UFT_DETECT_D71:      return "d71";
        case UFT_DETECT_D81:      return "d81";
        case UFT_DETECT_D80:      return "d80";
        case UFT_DETECT_D82:      return "d82";
        case UFT_DETECT_G64:      return "g64";
        case UFT_DETECT_G71:      return "g71";
        case UFT_DETECT_ADF:      return "adf";
        case UFT_DETECT_IMG:      return "img";
        case UFT_DETECT_ST:       return "st";
        case UFT_DETECT_MSA:      return "msa";
        case UFT_DETECT_DSK_APPLE: return "dsk";
        case UFT_DETECT_NIB:      return "nib";
        case UFT_DETECT_A2R:      return "a2r";
        case UFT_DETECT_SCP:      return "scp";
        case UFT_DETECT_HFE:      return "hfe";
        case UFT_DETECT_IPF:      return "ipf";
        case UFT_DETECT_KRYOFLUX: return "raw";
        case UFT_DETECT_TD0:      return "td0";
        case UFT_DETECT_IMD:      return "imd";
        case UFT_DETECT_FDI:      return "fdi";
        default:                  return "";
    }
}

bool uft_format_is_writable(uft_detect_format_t format) {
    switch (format) {
        case UFT_DETECT_D64:
        case UFT_DETECT_D71:
        case UFT_DETECT_D81:
        case UFT_DETECT_G64:
        case UFT_DETECT_ADF:
        case UFT_DETECT_IMG:
        case UFT_DETECT_ST:
        case UFT_DETECT_SCP:
        case UFT_DETECT_HFE:
        case UFT_DETECT_A2R:
            return true;
        default:
            return false;
    }
}

bool uft_format_is_flux(uft_detect_format_t format) {
    switch (format) {
        case UFT_DETECT_SCP:
        case UFT_DETECT_HFE:
        case UFT_DETECT_IPF:
        case UFT_DETECT_KRYOFLUX:
        case UFT_DETECT_A2R:
            return true;
        default:
            return false;
    }
}

#endif // UFT_FORMAT_DETECT_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // UFT_FORMAT_DETECT_H
