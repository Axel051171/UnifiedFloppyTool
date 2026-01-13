/**
 * @file uft_platform_hint.c
 * @brief Platform Hint for Format Detection Disambiguation
 * 
 * AUDIT FIX: TODO in uft_format_detect.c:557
 * 
 * Uses platform hints to disambiguate between similar formats
 * (e.g., Amiga ADF vs Atari ST image with same size).
 */

#include "uft/uft_detect.h"
#include "uft/uft_platform.h"
#include <string.h>
#include "uft/uft_compat.h"

// ============================================================================
// PLATFORM DEFINITIONS
// ============================================================================

typedef enum {
    UFT_PLATFORM_UNKNOWN = 0,
    UFT_PLATFORM_COMMODORE,    // C64, C128, VIC-20, PET
    UFT_PLATFORM_AMIGA,
    UFT_PLATFORM_ATARI_8BIT,   // 400/800/XL/XE
    UFT_PLATFORM_ATARI_ST,
    UFT_PLATFORM_APPLE_II,
    UFT_PLATFORM_APPLE_MAC,
    UFT_PLATFORM_IBM_PC,
    UFT_PLATFORM_MSX,
    UFT_PLATFORM_BBC,
    UFT_PLATFORM_AMSTRAD,
    UFT_PLATFORM_SPECTRUM,
    UFT_PLATFORM_TRS80,
    UFT_PLATFORM_PC98,
    UFT_PLATFORM_FM_TOWNS,
    UFT_PLATFORM_X68000,
    UFT_PLATFORM_COUNT
} uft_platform_t;

// ============================================================================
// PLATFORM DETECTION FROM CONTENT
// ============================================================================

/**
 * @brief Detect platform from bootblock/directory content
 */
static uft_platform_t detect_platform_from_content(const uint8_t* data, size_t size) {
    if (!data || size < 3) return UFT_PLATFORM_UNKNOWN;
    
    // Amiga bootblock
    if (size >= 4 && data[0] == 'D' && data[1] == 'O' && data[2] == 'S') {
        return UFT_PLATFORM_AMIGA;
    }
    
    // Commodore BAM signature (track 18, sector 0)
    // First two bytes are track/sector link, then 'A' for format version
    if (size >= 256) {
        // Look for CBM DOS signature at various offsets
        if (size >= 91392 + 256) {  // D64 BAM position
            const uint8_t* bam = data + 91392;
            if (bam[0] == 18 && bam[1] == 1) {
                return UFT_PLATFORM_COMMODORE;
            }
        }
    }
    
    // Atari ST bootblock
    if (size >= 512) {
        // Check for 68000 branch instruction at start
        if ((data[0] == 0x60 || data[0] == 0x00) && 
            (data[1] == 0x00 || data[1] == 0x3C)) {
            // Check for Atari serial number area
            if (data[8] == 0 && data[9] == 0) {
                // Could be Atari ST - need more checks
            }
        }
        
        // Check BPB for Atari vs PC
        if (data[0] == 0xEB || data[0] == 0xE9) {
            // PC boot
            return UFT_PLATFORM_IBM_PC;
        }
    }
    
    // Apple II DOS 3.3 / ProDOS
    if (size >= 256) {
        // VTOC typically at track 17, sector 0
        // First byte is track of first catalog sector
        if (data[0] == 17 && data[1] < 16) {
            return UFT_PLATFORM_APPLE_II;
        }
    }
    
    // BBC Micro
    if (size >= 512) {
        // Check for DFS catalog
        if (memcmp(data, "\x00\x00", 2) == 0) {
            // Could be BBC - check sector count byte
        }
    }
    
    return UFT_PLATFORM_UNKNOWN;
}

/**
 * @brief Detect platform from file extension
 */
static uft_platform_t detect_platform_from_extension(const char* filename) {
    if (!filename) return UFT_PLATFORM_UNKNOWN;
    
    const char* ext = strrchr(filename, '.');
    if (!ext) return UFT_PLATFORM_UNKNOWN;
    ext++;  // Skip dot
    
    // Commodore
    if (strcasecmp(ext, "d64") == 0 || strcasecmp(ext, "d71") == 0 ||
        strcasecmp(ext, "d81") == 0 || strcasecmp(ext, "g64") == 0 ||
        strcasecmp(ext, "nib") == 0 || strcasecmp(ext, "t64") == 0) {
        return UFT_PLATFORM_COMMODORE;
    }
    
    // Amiga
    if (strcasecmp(ext, "adf") == 0 || strcasecmp(ext, "adz") == 0 ||
        strcasecmp(ext, "dms") == 0) {
        return UFT_PLATFORM_AMIGA;
    }
    
    // Atari ST
    if (strcasecmp(ext, "st") == 0 || strcasecmp(ext, "msa") == 0 ||
        strcasecmp(ext, "stx") == 0) {
        return UFT_PLATFORM_ATARI_ST;
    }
    
    // Atari 8-bit
    if (strcasecmp(ext, "atr") == 0 || strcasecmp(ext, "xfd") == 0 ||
        strcasecmp(ext, "dcm") == 0) {
        return UFT_PLATFORM_ATARI_8BIT;
    }
    
    // Apple
    if (strcasecmp(ext, "do") == 0 || strcasecmp(ext, "po") == 0 ||
        strcasecmp(ext, "dsk") == 0 || strcasecmp(ext, "woz") == 0 ||
        strcasecmp(ext, "2mg") == 0) {
        return UFT_PLATFORM_APPLE_II;
    }
    
    // PC
    if (strcasecmp(ext, "img") == 0 || strcasecmp(ext, "ima") == 0 ||
        strcasecmp(ext, "vfd") == 0 || strcasecmp(ext, "flp") == 0) {
        return UFT_PLATFORM_IBM_PC;
    }
    
    // BBC Micro
    if (strcasecmp(ext, "ssd") == 0 || strcasecmp(ext, "dsd") == 0) {
        return UFT_PLATFORM_BBC;
    }
    
    // Amstrad
    if (strcasecmp(ext, "dsk") == 0) {
        // Ambiguous - could be Amstrad or Apple
        return UFT_PLATFORM_UNKNOWN;
    }
    
    // MSX
    if (strcasecmp(ext, "dsk") == 0) {
        return UFT_PLATFORM_MSX;  // Also ambiguous
    }
    
    return UFT_PLATFORM_UNKNOWN;
}

// ============================================================================
// FORMAT SCORING ADJUSTMENT
// ============================================================================

/**
 * @brief Adjust detection confidence based on platform hint
 * 
 * @param format Detected format
 * @param base_confidence Original confidence (0-100)
 * @param hint Platform hint
 * @return Adjusted confidence
 */
int uft_adjust_confidence_by_platform(uft_format_t format, int base_confidence,
                                       uft_platform_t hint) {
    if (hint == UFT_PLATFORM_UNKNOWN) {
        return base_confidence;
    }
    
    // Format-to-platform mapping
    uft_platform_t format_platform = UFT_PLATFORM_UNKNOWN;
    
    switch (format) {
        case UFT_FORMAT_D64:
        case UFT_FORMAT_D71:
        case UFT_FORMAT_D81:
        case UFT_FORMAT_G64:
            format_platform = UFT_PLATFORM_COMMODORE;
            break;
            
        case UFT_FORMAT_ADF:
            format_platform = UFT_PLATFORM_AMIGA;
            break;
            
        case UFT_FORMAT_ST:
        case UFT_FORMAT_MSA:
        case UFT_FORMAT_STX:
            format_platform = UFT_PLATFORM_ATARI_ST;
            break;
            
        case UFT_FORMAT_ATR:
        case UFT_FORMAT_XFD:
            format_platform = UFT_PLATFORM_ATARI_8BIT;
            break;
            
        case UFT_FORMAT_DO:
        case UFT_FORMAT_PO:
        case UFT_FORMAT_WOZ:
        case UFT_FORMAT_NIB:
            format_platform = UFT_PLATFORM_APPLE_II;
            break;
            
        case UFT_FORMAT_IMG:
            format_platform = UFT_PLATFORM_IBM_PC;
            break;
            
        case UFT_FORMAT_SSD:
        case UFT_FORMAT_DSD:
            format_platform = UFT_PLATFORM_BBC;
            break;
            
        default:
            break;
    }
    
    if (format_platform == UFT_PLATFORM_UNKNOWN) {
        return base_confidence;
    }
    
    // Adjust confidence
    if (format_platform == hint) {
        // Platform matches - boost confidence
        return base_confidence + 10;
    } else if (hint != UFT_PLATFORM_UNKNOWN) {
        // Platform mismatch - reduce confidence
        return base_confidence - 15;
    }
    
    return base_confidence;
}

/**
 * @brief Main disambiguation function
 * 
 * @param data File data
 * @param size Data size
 * @param filename Filename (for extension hint)
 * @param candidates Array of candidate formats
 * @param scores Array of scores (modified in place)
 * @param count Number of candidates
 */
void uft_apply_platform_hints(const uint8_t* data, size_t size,
                               const char* filename,
                               const uft_format_t* candidates,
                               int* scores, int count) {
    // Get platform hints
    uft_platform_t ext_hint = detect_platform_from_extension(filename);
    uft_platform_t content_hint = detect_platform_from_content(data, size);
    
    // Prefer content hint over extension
    uft_platform_t hint = (content_hint != UFT_PLATFORM_UNKNOWN) 
                           ? content_hint : ext_hint;
    
    // Adjust all scores
    for (int i = 0; i < count; i++) {
        scores[i] = uft_adjust_confidence_by_platform(candidates[i], 
                                                       scores[i], hint);
    }
}
