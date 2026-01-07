// SPDX-License-Identifier: MIT
/*
 * emulator_formats.h - Floppy Emulator Formats
 * 
 * Formats designed for hardware floppy emulators:
 * - HFE: UFT HFE Format format
 * 
 * These formats bridge the gap between logical images and hardware,
 * allowing vintage computers to use modern storage via emulator devices.
 * 
 * @version 2.8.9
 * @date 2024-12-26
 */

#ifndef EMULATOR_FORMATS_H
#define EMULATOR_FORMATS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * EMULATOR FORMATS
 *============================================================================*/

/* HFE - UFT HFE Format */
#include "uft_hfe.h"

/*============================================================================*
 * FORMAT DETECTION
 *============================================================================*/

/**
 * @brief Emulator format types
 */
typedef enum {
    EMULATOR_FORMAT_UNKNOWN = 0,
    EMULATOR_FORMAT_HFE      /* UFT HFE Format */
} emulator_format_type_t;

/**
 * @brief Auto-detect emulator format from buffer
 * @param buffer File data buffer
 * @param size Buffer size in bytes
 * @return Detected format type
 */
static inline emulator_format_type_t emulator_detect_format(const uint8_t* buffer, size_t size)
{
    if (!buffer || size < 16) return EMULATOR_FORMAT_UNKNOWN;
    
    /* HFE: "HXCPICFE" signature */
    if (size >= 8 && 
        buffer[0] == 'H' && buffer[1] == 'X' && buffer[2] == 'C' && 
        buffer[3] == 'P' && buffer[4] == 'I' && buffer[5] == 'C' &&
        buffer[6] == 'F' && buffer[7] == 'E') {
        return EMULATOR_FORMAT_HFE;
    }
    
    return EMULATOR_FORMAT_UNKNOWN;
}

/**
 * @brief Get format name string
 * @param fmt Format type
 * @return Format name
 */
static inline const char* emulator_format_name(emulator_format_type_t fmt)
{
    switch (fmt) {
        case EMULATOR_FORMAT_HFE: return "HFE (UFT HFE Format)";
        default: return "Unknown";
    }
}

/*============================================================================*
 * HARDWARE EMULATOR DEVICES
 *============================================================================*/

/**
 * @brief Popular hardware floppy emulators
 */
typedef struct {
    const char* name;
    const char* formats;
    const char* interfaces;
    const char* price_range;
} emulator_device_t;

static const emulator_device_t EMULATOR_DEVICES[] = {
    { "UFT HFE Format", "HFE, IMG, DSK, many", "Shugart, PC, Amiga", "$60-150" },
    { "Gotek (FlashFloppy)", "IMG, DSK, HFE, ADF", "PC, Amiga, Atari",   "$20-40" },
    { "FDADAP",              "Various",             "Apple II",           "$40" },
    { "Ultimate-II+",        "D64, D71, D81",       "C64/C128 cartridge", "$100+" },
    { NULL, NULL, NULL, NULL }
};

/*============================================================================*
 * HFE FORMAT NOTES
 *============================================================================*/

/**
 * HFE (UFT HFE Format) Format:
 * 
 * OVERVIEW:
 *   - Track-oriented container format
 *   - Stores MFM/FM encoded bitstreams
 *   - Used by HxC floppy emulator hardware
 *   - Supports multiple platforms
 * 
 * STRUCTURE:
 *   - Header: "HXCPICFE" signature
 *   - Track list: Offsets to track data
 *   - Track data: Per-track encoded bitstreams
 *   - Metadata: Geometry, encoding type, RPM, bit rate
 * 
 * FEATURES:
 *   ✅ Multi-platform support (PC, Amiga, Atari, C64, etc.)
 *   ✅ MFM and FM encoding
 *   ✅ Variable geometry
 *   ✅ Preserves track encoding
 *   ✅ Emulator-friendly structure
 * 
 * USE CASES:
 *   - Running vintage computers with modern storage
 *   - Testing disk images on real hardware
 *   - Preservation without original drives
 *   - Cross-platform disk image interchange
 * 
 * WORKFLOW:
 *   Logical Image → HFE → HxC Emulator → Vintage Computer
 *   
 *   Example:
 *     IMG file → Convert to HFE → Load on HxC → Use on PC XT
 *     D64 file → Convert to HFE → Load on HxC → Use on C64
 *     ADF file → Convert to HFE → Load on HxC → Use on Amiga
 * 
 * LIMITATIONS:
 *   ⚠️  Sector-level R/W not always meaningful
 *        (data stored as encoded bitstreams)
 *   ⚠️  Requires encoding knowledge for conversion
 *   ⚠️  Copy protection support varies
 * 
 * ADVANTAGES:
 *   ✅ Works on real hardware
 *   ✅ No original drives needed
 *   ✅ Multi-platform support
 *   ✅ Active community
 *   ✅ Affordable hardware ($60-150)
 * 
 * CRITICAL FOR:
 *   - Testing preservation work on real hardware
 *   - Vintage computer enthusiasts
 *   - Museum exhibits (safe, reliable)
 *   - Development/debugging on real systems
 */

/*============================================================================*
 * WHY EMULATOR FORMATS MATTER
 *============================================================================*/

/**
 * EMULATOR FORMATS - THE MISSING LINK:
 * 
 * Problem: Vintage computers need floppy drives
 *   - Original drives failing/dead
 *   - Floppy disks deteriorating
 *   - Mechanical complexity
 *   - Alignment issues
 *   - Media availability
 * 
 * Solution: Hardware floppy emulators
 *   - Replace floppy drive with emulator
 *   - Use USB/SD card for storage
 *   - Load disk images from modern media
 *   - Computer "thinks" it has real floppy
 * 
 * WHY HFE FORMAT:
 *   ✅ Hardware emulator standard
 *   ✅ Multi-platform support
 *   ✅ Preserves encoding (MFM/FM)
 *   ✅ Works on real computers
 *   ✅ Active community & support
 * 
 * PRESERVATION VALUE:
 *   - Test preserved images on real hardware
 *   - Verify preservation quality
 *   - Enable vintage computing without wear
 *   - Bridge digital and physical
 * 
 * REAL-WORLD IMPACT:
 *   Museums: Run exhibits safely
 *   Collectors: Use computers without damage
 *   Developers: Test on real hardware
 *   Archivists: Verify preservation work
 */

#ifdef __cplusplus
}
#endif

#endif /* EMULATOR_FORMATS_H */
