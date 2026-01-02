/**
 * @file uft_format_advisor.c
 * @brief Format Advisor - Empfiehlt Formate basierend auf Image-Inhalt
 * 
 * LAYER SEPARATION:
 * - Format-Logik gehört in Core, NICHT in Hardware
 * - Hardware stellt nur fest: "Ich kann Flux lesen"
 * - Core entscheidet: "Bei Commodore-Drive → G64 empfohlen"
 */

#include "uft/uft_format_advisor.h"
#include "uft/uft_unified_image.h"
#include "uft/uft_safe.h"
#include <string.h>

// ============================================================================
// Internal Detection Helpers
// ============================================================================

typedef struct {
    uft_format_t    format;
    int             score;
    const char*     reason;
} format_candidate_t;

static int detect_commodore_format(const uft_unified_image_t* img) {
    // Commodore: 35/40/70/80 Tracks, 256-byte Sektoren
    if (img->geometry.cylinders == 35 || img->geometry.cylinders == 40) {
        if (img->geometry.heads == 1) {
            // Check for variable sectors per track (1541 signature)
            return 90;
        }
    }
    if (img->geometry.cylinders == 70 || img->geometry.cylinders == 80) {
        if (img->geometry.heads == 2) {
            return 85;  // 1571
        }
    }
    return 0;
}

static int detect_amiga_format(const uft_unified_image_t* img) {
    // Amiga: 80 Tracks, 2 Heads, 11 Sektoren (DD) oder 22 (HD)
    if (img->geometry.cylinders == 80 && img->geometry.heads == 2) {
        // Check sector count per track
        if (img->sector_meta.encoding == UFT_ENCODING_MFM) {
            return 85;
        }
    }
    return 0;
}

static int detect_pc_format(const uft_unified_image_t* img) {
    // PC: 40/80 Tracks, 1-2 Heads, 8-36 Sektoren, 512-byte
    if (img->geometry.cylinders == 40 || img->geometry.cylinders == 80) {
        if (img->sector_meta.encoding == UFT_ENCODING_MFM) {
            return 80;
        }
    }
    return 0;
}

static int detect_apple_format(const uft_unified_image_t* img) {
    // Apple II: 35 Tracks, 1 Head, 13 oder 16 Sektoren, 256-byte
    if (img->geometry.cylinders == 35 && img->geometry.heads == 1) {
        if (img->sector_meta.encoding == UFT_ENCODING_GCR_APPLE) {
            return 90;
        }
    }
    return 0;
}

// ============================================================================
// Public API
// ============================================================================

uft_error_t uft_get_format_advice(const uft_unified_image_t* img,
                                   uft_format_advice_t* advice) {
    if (!img || !advice) return UFT_ERROR_NULL_POINTER;
    
    memset(advice, 0, sizeof(*advice));
    
    // Sammle Kandidaten
    format_candidate_t candidates[16];
    int candidate_count = 0;
    
    // Flux-basierte Images → native Flux-Formate bevorzugen
    if (uft_image_has_layer(img, UFT_LAYER_FLUX)) {
        candidates[candidate_count++] = (format_candidate_t){
            .format = UFT_FORMAT_SCP,
            .score = 95,
            .reason = "Flux data available - SCP preserves all transitions"
        };
        candidates[candidate_count++] = (format_candidate_t){
            .format = UFT_FORMAT_HFE,
            .score = 90,
            .reason = "HFE compatible with most emulators"
        };
    }
    
    // Encoding-basierte Empfehlungen
    switch (img->sector_meta.encoding) {
        case UFT_ENCODING_GCR_CBM:
            candidates[candidate_count++] = (format_candidate_t){
                .format = UFT_FORMAT_G64,
                .score = detect_commodore_format(img),
                .reason = "Commodore GCR encoding detected"
            };
            candidates[candidate_count++] = (format_candidate_t){
                .format = UFT_FORMAT_D64,
                .score = detect_commodore_format(img) - 10,
                .reason = "Sector-level Commodore format"
            };
            break;
            
        case UFT_ENCODING_GCR_APPLE:
            candidates[candidate_count++] = (format_candidate_t){
                .format = UFT_FORMAT_WOZ,
                .score = detect_apple_format(img),
                .reason = "Apple GCR encoding - WOZ preserves protection"
            };
            candidates[candidate_count++] = (format_candidate_t){
                .format = UFT_FORMAT_NIB,
                .score = detect_apple_format(img) - 5,
                .reason = "Apple nibble format"
            };
            break;
            
        case UFT_ENCODING_MFM:
            if (detect_amiga_format(img) > 0) {
                candidates[candidate_count++] = (format_candidate_t){
                    .format = UFT_FORMAT_ADF,
                    .score = detect_amiga_format(img),
                    .reason = "Amiga MFM geometry detected"
                };
            }
            if (detect_pc_format(img) > 0) {
                candidates[candidate_count++] = (format_candidate_t){
                    .format = UFT_FORMAT_IMG,
                    .score = detect_pc_format(img),
                    .reason = "IBM PC MFM geometry detected"
                };
            }
            break;
            
        case UFT_ENCODING_FM:
            candidates[candidate_count++] = (format_candidate_t){
                .format = UFT_FORMAT_IMD,
                .score = 80,
                .reason = "FM encoding - IMD preserves SD data"
            };
            break;
            
        default:
            break;
    }
    
    // Fallback wenn nichts erkannt
    if (candidate_count == 0) {
        candidates[candidate_count++] = (format_candidate_t){
            .format = UFT_FORMAT_IMG,
            .score = 50,
            .reason = "Generic sector image (fallback)"
        };
    }
    
    // Sortiere nach Score (einfaches Bubble-Sort)
    for (int i = 0; i < candidate_count - 1; i++) {
        for (int j = i + 1; j < candidate_count; j++) {
            if (candidates[j].score > candidates[i].score) {
                format_candidate_t tmp = candidates[i];
                candidates[i] = candidates[j];
                candidates[j] = tmp;
            }
        }
    }
    
    // Ergebnis füllen
    advice->recommended_format = candidates[0].format;
    advice->confidence = candidates[0].score;
    advice->reason = candidates[0].reason;
    
    advice->alternative_count = 0;
    for (int i = 1; i < candidate_count && advice->alternative_count < 8; i++) {
        advice->alternatives[advice->alternative_count++] = candidates[i].format;
    }
    
    return UFT_OK;
}

// ============================================================================
// Conversion Compatibility
// ============================================================================

bool uft_format_can_convert(uft_format_t src, uft_format_t dst,
                             uft_conversion_info_t* info) {
    if (info) {
        memset(info, 0, sizeof(*info));
    }
    
    // Flux → Sector: Möglich, aber Informationsverlust
    bool src_is_flux = (src == UFT_FORMAT_SCP || src == UFT_FORMAT_KRYOFLUX ||
                        src == UFT_FORMAT_A2R || src == UFT_FORMAT_HFE);
    bool dst_is_flux = (dst == UFT_FORMAT_SCP || dst == UFT_FORMAT_KRYOFLUX ||
                        dst == UFT_FORMAT_A2R || dst == UFT_FORMAT_HFE);
    bool dst_is_sector = (dst == UFT_FORMAT_IMG || dst == UFT_FORMAT_D64 ||
                          dst == UFT_FORMAT_ADF || dst == UFT_FORMAT_DSK);
    
    if (src_is_flux && dst_is_sector) {
        if (info) {
            info->possible = true;
            info->lossy = true;
            snprintf(info->warning, sizeof(info->warning),
                     "Flux timing data will be lost");
        }
        return true;
    }
    
    // Sector → Flux: Möglich durch Synthese
    bool src_is_sector = (src == UFT_FORMAT_IMG || src == UFT_FORMAT_D64 ||
                          src == UFT_FORMAT_ADF || src == UFT_FORMAT_DSK);
    
    if (src_is_sector && dst_is_flux) {
        if (info) {
            info->possible = true;
            info->lossy = false;
            snprintf(info->warning, sizeof(info->warning),
                     "Flux will be synthesized from sectors");
        }
        return true;
    }
    
    // Gleicher Typ: OK
    if (src == dst) {
        if (info) {
            info->possible = true;
            info->lossy = false;
        }
        return true;
    }
    
    // Default: erlaubt
    if (info) {
        info->possible = true;
        info->lossy = false;
    }
    return true;
}

// ============================================================================
// Format Info
// ============================================================================

const char* uft_format_get_name(uft_format_t format) {
    switch (format) {
        case UFT_FORMAT_IMG: return "Raw Sector Image";
        case UFT_FORMAT_D64: return "Commodore D64";
        case UFT_FORMAT_G64: return "Commodore G64";
        case UFT_FORMAT_ADF: return "Amiga ADF";
        case UFT_FORMAT_SCP: return "SuperCard Pro";
        case UFT_FORMAT_HFE: return "HxC Floppy Emulator";
        case UFT_FORMAT_KRYOFLUX: return "KryoFlux Stream";
        case UFT_FORMAT_WOZ: return "WOZ (Applesauce)";
        case UFT_FORMAT_NIB: return "Apple Nibble";
        case UFT_FORMAT_IMD: return "ImageDisk";
        case UFT_FORMAT_DSK: return "Amstrad DSK";
        case UFT_FORMAT_A2R: return "A2R (Applesauce)";
        default: return "Unknown";
    }
}

const char* uft_format_get_extension(uft_format_t format) {
    switch (format) {
        case UFT_FORMAT_IMG: return ".img";
        case UFT_FORMAT_D64: return ".d64";
        case UFT_FORMAT_G64: return ".g64";
        case UFT_FORMAT_ADF: return ".adf";
        case UFT_FORMAT_SCP: return ".scp";
        case UFT_FORMAT_HFE: return ".hfe";
        case UFT_FORMAT_KRYOFLUX: return ".raw";
        case UFT_FORMAT_WOZ: return ".woz";
        case UFT_FORMAT_NIB: return ".nib";
        case UFT_FORMAT_IMD: return ".imd";
        case UFT_FORMAT_DSK: return ".dsk";
        case UFT_FORMAT_A2R: return ".a2r";
        default: return "";
    }
}

bool uft_format_supports_flux(uft_format_t format) {
    switch (format) {
        case UFT_FORMAT_SCP:
        case UFT_FORMAT_HFE:
        case UFT_FORMAT_KRYOFLUX:
        case UFT_FORMAT_A2R:
        case UFT_FORMAT_WOZ:
            return true;
        default:
            return false;
    }
}
