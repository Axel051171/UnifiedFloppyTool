/**
 * @file uft_types.c
 * @brief UnifiedFloppyTool - Type Implementations
 * 
 * Implementiert Geometry-Presets, Format-Informationen und
 * Utility-Funktionen für die Basis-Typen.
 * 
 * @author UFT Team
 * @date 2025
 */

#include "uft/uft_types.h"
#include <string.h>

// ============================================================================
// Geometry Preset Table
// ============================================================================

/**
 * @brief Statische Tabelle aller Geometrie-Presets
 */
static const uft_geometry_t geometry_presets[UFT_GEO_MAX] = {
    // UFT_GEO_UNKNOWN
    {
        .cylinders = 0,
        .heads = 0,
        .sectors = 0,
        .sector_size = 0,
        .total_sectors = 0,
        .double_step = false
    },
    
    // === PC Formate ===
    
    // UFT_GEO_PC_360K - 5.25" DD
    {
        .cylinders = 40,
        .heads = 2,
        .sectors = 9,
        .sector_size = 512,
        .total_sectors = 40 * 2 * 9,  // 720 = 360KB
        .double_step = false
    },
    
    // UFT_GEO_PC_720K - 3.5" DD
    {
        .cylinders = 80,
        .heads = 2,
        .sectors = 9,
        .sector_size = 512,
        .total_sectors = 80 * 2 * 9,  // 1440 = 720KB
        .double_step = false
    },
    
    // UFT_GEO_PC_1200K - 5.25" HD
    {
        .cylinders = 80,
        .heads = 2,
        .sectors = 15,
        .sector_size = 512,
        .total_sectors = 80 * 2 * 15,  // 2400 = 1.2MB
        .double_step = false
    },
    
    // UFT_GEO_PC_1440K - 3.5" HD
    {
        .cylinders = 80,
        .heads = 2,
        .sectors = 18,
        .sector_size = 512,
        .total_sectors = 80 * 2 * 18,  // 2880 = 1.44MB
        .double_step = false
    },
    
    // UFT_GEO_PC_2880K - 3.5" ED
    {
        .cylinders = 80,
        .heads = 2,
        .sectors = 36,
        .sector_size = 512,
        .total_sectors = 80 * 2 * 36,  // 5760 = 2.88MB
        .double_step = false
    },
    
    // === Amiga Formate ===
    
    // UFT_GEO_AMIGA_DD - 880KB
    {
        .cylinders = 80,
        .heads = 2,
        .sectors = 11,
        .sector_size = 512,
        .total_sectors = 80 * 2 * 11,  // 1760 = 880KB
        .double_step = false
    },
    
    // UFT_GEO_AMIGA_HD - 1.76MB
    {
        .cylinders = 80,
        .heads = 2,
        .sectors = 22,
        .sector_size = 512,
        .total_sectors = 80 * 2 * 22,  // 3520 = 1.76MB
        .double_step = false
    },
    
    // === C64 Formate ===
    
    // UFT_GEO_C64_1541 - 170KB (variable sectors per track)
    {
        .cylinders = 35,
        .heads = 1,
        .sectors = 17,  // Average, actually 17-21 per track
        .sector_size = 256,
        .total_sectors = 683,  // Actual total
        .double_step = false
    },
    
    // UFT_GEO_C64_1571 - 340KB (double-sided 1541)
    {
        .cylinders = 35,
        .heads = 2,
        .sectors = 17,
        .sector_size = 256,
        .total_sectors = 1366,
        .double_step = false
    },
    
    // === Apple Formate ===
    
    // UFT_GEO_APPLE_DOS - 140KB (16 sectors)
    {
        .cylinders = 35,
        .heads = 1,
        .sectors = 16,
        .sector_size = 256,
        .total_sectors = 35 * 16,  // 560 = 140KB
        .double_step = false
    },
    
    // UFT_GEO_APPLE_PRODOS - 140KB (same geometry, different filesystem)
    {
        .cylinders = 35,
        .heads = 1,
        .sectors = 16,
        .sector_size = 256,
        .total_sectors = 35 * 16,
        .double_step = false
    },
    
    // UFT_GEO_APPLE_400K - 3.5" SS (variable)
    {
        .cylinders = 80,
        .heads = 1,
        .sectors = 10,  // Average, 8-12 per zone
        .sector_size = 512,
        .total_sectors = 800,  // Actual total
        .double_step = false
    },
    
    // UFT_GEO_APPLE_800K - 3.5" DS
    {
        .cylinders = 80,
        .heads = 2,
        .sectors = 10,
        .sector_size = 512,
        .total_sectors = 1600,
        .double_step = false
    },
    
    // === Atari Formate ===
    
    // UFT_GEO_ATARI_SS_SD - 90KB (single density)
    {
        .cylinders = 40,
        .heads = 1,
        .sectors = 18,
        .sector_size = 128,
        .total_sectors = 40 * 18,  // 720 = 90KB
        .double_step = false
    },
    
    // UFT_GEO_ATARI_SS_DD - 180KB (double density)
    {
        .cylinders = 40,
        .heads = 1,
        .sectors = 18,
        .sector_size = 256,
        .total_sectors = 40 * 18,  // 720 = 180KB
        .double_step = false
    },
    
    // UFT_GEO_ATARI_ST_SS - 360KB
    {
        .cylinders = 80,
        .heads = 1,
        .sectors = 9,
        .sector_size = 512,
        .total_sectors = 80 * 9,  // 720 = 360KB
        .double_step = false
    },
    
    // UFT_GEO_ATARI_ST_DS - 720KB
    {
        .cylinders = 80,
        .heads = 2,
        .sectors = 9,
        .sector_size = 512,
        .total_sectors = 80 * 2 * 9,  // 1440 = 720KB
        .double_step = false
    },
};

uft_geometry_t uft_geometry_for_preset(uft_geometry_preset_t preset) {
    if (preset >= 0 && preset < UFT_GEO_MAX) {
        return geometry_presets[preset];
    }
    return geometry_presets[UFT_GEO_UNKNOWN];
}

// ============================================================================
// Format Info Table
// ============================================================================

/**
 * @brief Statische Tabelle aller Format-Informationen
 */
static const uft_format_info_t format_info_table[UFT_FORMAT_MAX] = {
    // UFT_FORMAT_UNKNOWN
    {
        .format = UFT_FORMAT_UNKNOWN,
        .name = "Unknown",
        .description = "Unknown or unsupported format",
        .extensions = "",
        .has_flux = false,
        .can_write = false,
        .preserves_timing = false
    },
    
    // === Sektor-Images ===
    
    // UFT_FORMAT_RAW
    {
        .format = UFT_FORMAT_RAW,
        .name = "RAW",
        .description = "Raw sector dump",
        .extensions = "raw;bin",
        .has_flux = false,
        .can_write = true,
        .preserves_timing = false
    },
    
    // UFT_FORMAT_IMG
    {
        .format = UFT_FORMAT_IMG,
        .name = "IMG",
        .description = "Generic PC disk image",
        .extensions = "img;ima;dsk;vfd",
        .has_flux = false,
        .can_write = true,
        .preserves_timing = false
    },
    
    // UFT_FORMAT_ADF
    {
        .format = UFT_FORMAT_ADF,
        .name = "ADF",
        .description = "Amiga Disk File",
        .extensions = "adf;adz",
        .has_flux = false,
        .can_write = true,
        .preserves_timing = false
    },
    
    // UFT_FORMAT_D64
    {
        .format = UFT_FORMAT_D64,
        .name = "D64",
        .description = "Commodore 64 disk image",
        .extensions = "d64",
        .has_flux = false,
        .can_write = true,
        .preserves_timing = false
    },
    
    // UFT_FORMAT_DSK
    {
        .format = UFT_FORMAT_DSK,
        .name = "DSK",
        .description = "Generic DSK format",
        .extensions = "dsk",
        .has_flux = false,
        .can_write = true,
        .preserves_timing = false
    },
    
    // UFT_FORMAT_ST
    {
        .format = UFT_FORMAT_ST,
        .name = "ST",
        .description = "Atari ST disk image",
        .extensions = "st",
        .has_flux = false,
        .can_write = true,
        .preserves_timing = false
    },
    
    // UFT_FORMAT_MSA
    {
        .format = UFT_FORMAT_MSA,
        .name = "MSA",
        .description = "Atari MSA (compressed)",
        .extensions = "msa",
        .has_flux = false,
        .can_write = true,
        .preserves_timing = false
    },
    
    // === Flux-Images ===
    
    // UFT_FORMAT_SCP
    {
        .format = UFT_FORMAT_SCP,
        .name = "SCP",
        .description = "SuperCard Pro flux image",
        .extensions = "scp",
        .has_flux = true,
        .can_write = true,
        .preserves_timing = true
    },
    
    // UFT_FORMAT_UFT_KF_STREAM
    {
        .format = UFT_FORMAT_UFT_KF_STREAM,
        .name = "KryoFlux",
        .description = "KryoFlux stream files",
        .extensions = "raw",  // trackXX.Y.raw
        .has_flux = true,
        .can_write = false,  // Read-only
        .preserves_timing = true
    },
    
    // UFT_FORMAT_HFE
    {
        .format = UFT_FORMAT_HFE,
        .name = "HFE",
        .description = "UFT HFE Format",
        .extensions = "hfe",
        .has_flux = true,
        .can_write = true,
        .preserves_timing = true
    },
    
    // UFT_FORMAT_IPF
    {
        .format = UFT_FORMAT_IPF,
        .name = "IPF",
        .description = "Interchangeable Preservation Format",
        .extensions = "ipf",
        .has_flux = true,
        .can_write = false,  // CAPS library needed
        .preserves_timing = true
    },
    
    // UFT_FORMAT_CT_RAW
    {
        .format = UFT_FORMAT_CT_RAW,
        .name = "CatWeasel",
        .description = "CatWeasel raw format",
        .extensions = "ctr",
        .has_flux = true,
        .can_write = false,
        .preserves_timing = true
    },
    
    // UFT_FORMAT_A2R
    {
        .format = UFT_FORMAT_A2R,
        .name = "A2R",
        .description = "Applesauce A2R format",
        .extensions = "a2r",
        .has_flux = true,
        .can_write = true,
        .preserves_timing = true
    },
    
    // === Spezial ===
    
    // UFT_FORMAT_G64
    {
        .format = UFT_FORMAT_G64,
        .name = "G64",
        .description = "Commodore 64 GCR image",
        .extensions = "g64",
        .has_flux = false,  // GCR-encoded, but not raw flux
        .can_write = true,
        .preserves_timing = false
    },
    
    // UFT_FORMAT_NIB
    {
        .format = UFT_FORMAT_NIB,
        .name = "NIB",
        .description = "Apple nibble format",
        .extensions = "nib",
        .has_flux = false,
        .can_write = true,
        .preserves_timing = false
    },
    
    // UFT_FORMAT_FDI
    {
        .format = UFT_FORMAT_FDI,
        .name = "FDI",
        .description = "Formatted Disk Image",
        .extensions = "fdi",
        .has_flux = false,
        .can_write = true,
        .preserves_timing = false
    },
    
    // UFT_FORMAT_TD0
    {
        .format = UFT_FORMAT_TD0,
        .name = "TD0",
        .description = "Teledisk archive",
        .extensions = "td0",
        .has_flux = false,
        .can_write = false,
        .preserves_timing = false
    },
};

const uft_format_info_t* uft_format_get_info(uft_format_t format) {
    if (format >= 0 && format < UFT_FORMAT_MAX) {
        return &format_info_table[format];
    }
    return &format_info_table[UFT_FORMAT_UNKNOWN];
}

// ============================================================================
// Extension Lookup
// ============================================================================

/**
 * @brief Prüft ob Extension in Extension-Liste enthalten ist
 * 
 * Thread-safe Implementierung ohne strtok().
 * 
 * @param extensions Semikolon-getrennte Liste (z.B. "adf;adz")
 * @param ext Zu suchende Extension (ohne Punkt)
 * @return true wenn gefunden
 */
static bool extension_matches(const char* extensions, const char* ext) {
    if (!extensions || !ext || !*extensions || !*ext) {
        return false;
    }
    
    // Punkt am Anfang überspringen
    if (*ext == '.') {
        ext++;
    }
    
    size_t ext_len = strlen(ext);
    if (ext_len == 0 || ext_len > 16) {
        return false;  // Unrealistische Extension-Länge
    }
    
    // Thread-safe: Manuelle Tokenisierung ohne globalen State
    const char* p = extensions;
    while (*p) {
        // Führende Semikolons überspringen
        while (*p == ';') p++;
        if (!*p) break;
        
        // Token-Ende finden
        const char* end = p;
        while (*end && *end != ';') end++;
        
        size_t token_len = (size_t)(end - p);
        
        // Case-insensitive Vergleich
        if (token_len == ext_len) {
            bool match = true;
            for (size_t i = 0; i < token_len; i++) {
                char c1 = p[i];
                char c2 = ext[i];
                // Einfaches tolower für ASCII
                if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
                if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
                if (c1 != c2) {
                    match = false;
                    break;
                }
            }
            if (match) return true;
        }
        
        p = end;
    }
    
    return false;
}

uft_format_t uft_format_from_extension(const char* extension) {
    if (!extension || !*extension) {
        return UFT_FORMAT_UNKNOWN;
    }
    
    // Punkt überspringen
    if (*extension == '.') {
        extension++;
    }
    
    // Alle Formate durchsuchen
    for (int i = 1; i < UFT_FORMAT_MAX; i++) {
        if (extension_matches(format_info_table[i].extensions, extension)) {
            return (uft_format_t)i;
        }
    }
    
    return UFT_FORMAT_UNKNOWN;
}

// ============================================================================
// Geometry Utilities
// ============================================================================

/**
 * @brief Geometrie aus Dateigröße schätzen
 */
uft_geometry_preset_t uft_guess_geometry_from_size(size_t file_size) {
    // Exakte Matches zuerst
    switch (file_size) {
        // Amiga
        case 901120:   return UFT_GEO_AMIGA_DD;    // 880KB
        case 1802240:  return UFT_GEO_AMIGA_HD;    // 1.76MB
        
        // PC
        case 368640:   return UFT_GEO_PC_360K;     // 360KB
        case 737280:   return UFT_GEO_PC_720K;     // 720KB
        case 1228800:  return UFT_GEO_PC_1200K;    // 1.2MB
        case 1474560:  return UFT_GEO_PC_1440K;    // 1.44MB
        case 2949120:  return UFT_GEO_PC_2880K;    // 2.88MB
        
        // C64
        case 174848:   return UFT_GEO_C64_1541;    // D64 with errors
        case 175531:   return UFT_GEO_C64_1541;    // D64 standard
        case 349696:   return UFT_GEO_C64_1571;    // D71
        
        // Atari
        case 92160:    return UFT_GEO_ATARI_SS_SD; // 90KB
        case 184320:   return UFT_GEO_ATARI_SS_DD; // 180KB
        // Note: 368640 (360KB) is same as PC_360K, handled above
        
        // Apple
        case 143360:   return UFT_GEO_APPLE_DOS;   // 140KB
        case 409600:   return UFT_GEO_APPLE_400K;  // 400KB
        case 819200:   return UFT_GEO_APPLE_800K;  // 800KB
        
        default:
            break;
    }
    
    // Keine exakte Übereinstimmung
    return UFT_GEO_UNKNOWN;
}

/**
 * @brief Berechnet Gesamtgröße einer Geometrie in Bytes
 */
size_t uft_geometry_total_bytes(const uft_geometry_t* geo) {
    if (!geo) return 0;
    return (size_t)geo->total_sectors * geo->sector_size;
}

/**
 * @brief Validiert Geometrie
 */
bool uft_geometry_is_valid(const uft_geometry_t* geo) {
    if (!geo) return false;
    
    // Basis-Checks
    if (geo->cylinders == 0 || geo->cylinders > 255) return false;
    if (geo->heads == 0 || geo->heads > 2) return false;
    if (geo->sectors == 0 || geo->sectors > 255) return false;
    if (geo->sector_size == 0) return false;
    
    // Sector size muss Zweierpotenz sein (128, 256, 512, ...)
    if ((geo->sector_size & (geo->sector_size - 1)) != 0) return false;
    if (geo->sector_size < 128 || geo->sector_size > 8192) return false;
    
    return true;
}

/**
 * @brief Vergleicht zwei Geometrien
 */
bool uft_geometry_equals(const uft_geometry_t* a, const uft_geometry_t* b) {
    if (!a || !b) return false;
    
    return a->cylinders == b->cylinders &&
           a->heads == b->heads &&
           a->sectors == b->sectors &&
           a->sector_size == b->sector_size;
}

// ============================================================================
// Encoding Names
// ============================================================================

static const char* encoding_names[UFT_ENC_MAX] = {
    [UFT_ENC_UNKNOWN]      = "Unknown",
    [UFT_ENC_FM]           = "FM (Single Density)",
    [UFT_ENC_MFM]          = "MFM (Double Density)",
    [UFT_ENC_AMIGA_MFM]    = "Amiga MFM",
    [UFT_ENC_GCR_CBM]      = "GCR (Commodore)",
    [UFT_ENC_GCR_CBM_V]    = "GCR (Commodore, variant)",
    [UFT_ENC_GCR_APPLE_525] = "GCR (Apple 5.25\")",
    [UFT_ENC_GCR_APPLE_35] = "GCR (Apple 3.5\")",
    [UFT_ENC_MIXED]        = "Mixed Encoding",
};

const char* uft_encoding_name(uft_encoding_t encoding) {
    if (encoding >= 0 && encoding < UFT_ENC_MAX) {
        return encoding_names[encoding];
    }
    return "Unknown";
}

// ============================================================================
// Data Rate Calculation
// ============================================================================

/**
 * @brief Standard-Datenraten für verschiedene Geometrien (bits/sec)
 */
double uft_get_standard_data_rate(uft_geometry_preset_t preset) {
    switch (preset) {
        // FM Single Density
        case UFT_GEO_ATARI_SS_SD:
            return 125000.0;  // 125 kbit/s
        
        // MFM Double Density (250 kbit/s)
        case UFT_GEO_PC_360K:
        case UFT_GEO_PC_720K:
        case UFT_GEO_ATARI_SS_DD:
        case UFT_GEO_ATARI_ST_SS:
        case UFT_GEO_ATARI_ST_DS:
            return 250000.0;
        
        // Amiga DD (250 kbit/s, aber 11 Sektoren)
        case UFT_GEO_AMIGA_DD:
            return 250000.0;
        
        // MFM High Density (500 kbit/s)
        case UFT_GEO_PC_1200K:
        case UFT_GEO_PC_1440K:
        case UFT_GEO_AMIGA_HD:
            return 500000.0;
        
        // MFM Extra Density (1000 kbit/s)
        case UFT_GEO_PC_2880K:
            return 1000000.0;
        
        // GCR (variable)
        case UFT_GEO_C64_1541:
        case UFT_GEO_C64_1571:
            return 250000.0;  // Nominal, varies by zone
        
        // Apple GCR
        case UFT_GEO_APPLE_DOS:
        case UFT_GEO_APPLE_PRODOS:
            return 250000.0;
        
        case UFT_GEO_APPLE_400K:
        case UFT_GEO_APPLE_800K:
            return 500000.0;
        
        default:
            return 250000.0;  // Default DD
    }
}

/**
 * @brief Berechnet nominale Bit-Zeit in Nanosekunden
 */
double uft_get_nominal_bit_time_ns(uft_geometry_preset_t preset) {
    double rate = uft_get_standard_data_rate(preset);
    if (rate <= 0) return 4000.0;  // Default 4µs
    return 1e9 / rate;
}
