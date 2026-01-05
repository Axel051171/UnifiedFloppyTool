// SPDX-License-Identifier: MIT
/*
 * c64_formats.h - Commodore 64/128 Disk Formats
 * 
 * Complete Commodore disk image format support:
 * - D64: C64 1541 single-sided (existing)
 * - D71: C64 1571 double-sided (new!)
 * - D81: C64 1581 3.5" (future)
 * - G64: C64 GCR bitstream (existing)
 * - P64: C64 Pulse stream (existing)
 * 
 * Also includes GCR encoding/decoding support.
 * 
 * @version 2.8.9
 * @date 2024-12-26
 */

#ifndef C64_FORMATS_H
#define C64_FORMATS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * C64 DISK FORMATS
 *============================================================================*/

/* D64 - C64 1541 Single-Sided (may already exist in project) */
/* #include "d64.h" */

/* D71 - C64 1571 Double-Sided */
#include "uft_d71.h"

/* GCR - GCR encoding/decoding */
#include "c64_gcr.h"

/* G64 - GCR bitstream (if exists) */
/* #include "g64.h" */

/*============================================================================*
 * FORMAT DETECTION
 *============================================================================*/

/**
 * @brief C64 disk format types
 */
typedef enum {
    C64_FORMAT_UNKNOWN = 0,
    C64_FORMAT_D64,      /* 1541 single-sided */
    C64_FORMAT_D71,      /* 1571 double-sided */
    C64_FORMAT_D81,      /* 1581 3.5" (future) */
    C64_FORMAT_G64,      /* GCR bitstream */
    C64_FORMAT_P64       /* Pulse stream */
} c64_format_type_t;

/**
 * @brief Auto-detect C64 disk format from buffer and size
 * @param buffer File data buffer
 * @param size Buffer size in bytes
 * @return Detected format type
 */
static inline c64_format_type_t c64_detect_format(const uint8_t* buffer, size_t size)
{
    if (!buffer || size < 256) return C64_FORMAT_UNKNOWN;
    
    /* D71: Double-sided D64 */
    if (uft_d71_detect(buffer, size)) {
        return C64_FORMAT_D71;
    }
    
    /* D64: Standard sizes */
    if (size == 174848 || size == 175531 || size == 196608 || size == 197376) {
        return C64_FORMAT_D64;
    }
    
    /* D81: 3.5" format (future) */
    if (size == 819200 || size == 822400) {
        return C64_FORMAT_D81;
    }
    
    /* G64: GCR bitstream (has specific header) */
    if (size > 12 && buffer[0] == 'G' && buffer[1] == 'C' && buffer[2] == 'R') {
        return C64_FORMAT_G64;
    }
    
    return C64_FORMAT_UNKNOWN;
}

/**
 * @brief Get format name string
 * @param fmt Format type
 * @return Format name
 */
static inline const char* c64_format_name(c64_format_type_t fmt)
{
    switch (fmt) {
        case C64_FORMAT_D64: return "D64 (C64 1541 Single-Sided)";
        case C64_FORMAT_D71: return "D71 (C64 1571 Double-Sided)";
        case C64_FORMAT_D81: return "D81 (C64 1581 3.5\")";
        case C64_FORMAT_G64: return "G64 (C64 GCR Bitstream)";
        case C64_FORMAT_P64: return "P64 (C64 Pulse Stream)";
        default: return "Unknown";
    }
}

/*============================================================================*
 * C64 DRIVE GEOMETRIES
 *============================================================================*/

/**
 * @brief Commodore drive specifications
 */
typedef struct {
    const char* drive;
    const char* format;
    uint8_t tracks;
    uint8_t sides;
    uint32_t capacity_kb;
    const char* notes;
} c64_drive_spec_t;

static const c64_drive_spec_t C64_DRIVES[] = {
    { "1541",  "D64", 35, 1, 170, "Single-sided 5.25\", 4 speed zones, GCR" },
    { "1571",  "D71", 35, 2, 340, "Double-sided 5.25\", 4 speed zones, GCR" },
    { "1581",  "D81", 80, 2, 800, "3.5\" DS/DD, MFM encoding" },
    { "2040",  "D64", 35, 1, 170, "IEEE-488 interface, PET" },
    { "4040",  "D64", 35, 2, 340, "Dual drive, IEEE-488, PET" },
    { "8050",  "D80", 77, 1, 500, "5.25\" SS/QD, IEEE-488, PET" },
    { "8250",  "D82", 77, 2,1000, "5.25\" DS/QD, IEEE-488, PET" },
    { NULL, NULL, 0, 0, 0, NULL }
};

/*============================================================================*
 * GCR SPEED ZONES (1541/1571)
 *============================================================================*/

/**
 * @brief 1541/1571 speed zones
 * 
 * Commodore drives use 4 different rotation speeds
 * to optimize capacity across the disk surface.
 */
typedef struct {
    uint8_t zone;
    uint8_t first_track;
    uint8_t last_track;
    uint8_t sectors_per_track;
    uint16_t bytes_per_track;
} c64_speed_zone_t;

static const c64_speed_zone_t C64_SPEED_ZONES[] = {
    { 0,  1, 17, 21, 7820 },  /* Outer tracks - fastest */
    { 1, 18, 24, 19, 7170 },
    { 2, 25, 30, 18, 6300 },
    { 3, 31, 35, 17, 6020 },  /* Inner tracks - slowest */
    { 0,  0,  0,  0,    0 }
};

/*============================================================================*
 * D71 FORMAT NOTES
 *============================================================================*/

/**
 * D71 (Commodore 1571) Format:
 * 
 * OVERVIEW:
 *   - Double-sided extension of D64
 *   - Used by C64/C128 with 1571 drive
 *   - Two 35-track sides
 *   - Same GCR encoding as D64
 *   - Optional error-info table
 * 
 * STRUCTURE:
 *   - Side 0: Tracks 1-35 (same as D64)
 *   - Side 1: Tracks 36-70 (mirror of side 0)
 *   - Optional: 512-byte error table at end
 * 
 * SIZES:
 *   - Standard: 349,696 bytes (170KB × 2)
 *   - With error table: 350,208 bytes (+512)
 * 
 * FEATURES:
 *   ✅ Double capacity of D64
 *   ✅ Read/write both sides
 *   ✅ Same speed zones as D64
 *   ✅ Compatible with GCRRAW pipeline
 *   ✅ Optional error tracking
 * 
 * COMPATIBILITY:
 *   - C64 with 1571 drive
 *   - C128 (native drive)
 *   - Emulators: VICE, CCS64, etc.
 * 
 * USE CASES:
 *   - Larger programs/games
 *   - Data storage (double capacity)
 *   - CP/M on C128
 *   - Multi-disk compilations
 * 
 * PRESERVATION:
 *   ✅ Supported by UFT v2.8.9
 *   ✅ GCRRAW compatible
 *   ✅ Read/write/convert
 *   ✅ Error table support
 */

/*============================================================================*
 * C64 GCR ENCODING
 *============================================================================*/

/**
 * GCR (Group Code Recording) - Commodore Style:
 * 
 * ENCODING:
 *   - 4 data bits → 5 GCR bits
 *   - Ensures max 2 consecutive zeros
 *   - Self-clocking (no separate clock track)
 * 
 * SPEED ZONES:
 *   - Zone 0 (tracks  1-17): 21 sectors/track
 *   - Zone 1 (tracks 18-24): 19 sectors/track  
 *   - Zone 2 (tracks 25-30): 18 sectors/track
 *   - Zone 3 (tracks 31-35): 17 sectors/track
 * 
 * SECTOR FORMAT:
 *   - Sync bytes (0xFF)
 *   - Header (track, sector, ID, checksum)
 *   - Gap
 *   - Sync bytes
 *   - Data (256 bytes)
 *   - Checksum
 * 
 * ADVANTAGES:
 *   ✅ Self-clocking
 *   ✅ Error detection
 *   ✅ Variable capacity
 *   ✅ Reliable
 * 
 * CHALLENGES:
 *   ⚠️  Platform-specific
 *   ⚠️  Complex decoding
 *   ⚠️  Timing-sensitive
 */

#ifdef __cplusplus
}
#endif

#endif /* C64_FORMATS_H */
