// SPDX-License-Identifier: MIT
/*
 * hardware_formats.h - Hardware Flux Capture Formats (TIER 0)
 * 
 * PURE FLUX formats from actual floppy disk hardware:
 * - SCP: SuperCard Pro flux dumps (existing)
 * 
 * These are NOT normal disk images - they are RAW FLUX TRANSITIONS
 * from real hardware! Essential for museum-grade preservation.
 * 
 * @version 2.8.9
 * @date 2024-12-26
 */

#ifndef HARDWARE_FORMATS_H
#define HARDWARE_FORMATS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * HARDWARE FLUX FORMATS (TIER 0)
 *============================================================================*/

#include "uft_gwflux.h"

#include "uft_kfstream.h"

/* SCP - SuperCard Pro (already in project) */
/* #include "flux_format/scp.h" */

/*============================================================================*
 * FORMAT DETECTION
 *============================================================================*/

/**
 * @brief Hardware flux format types
 */
typedef enum {
    HARDWARE_FORMAT_UNKNOWN = 0,
    HARDWARE_FORMAT_SCP      /* SuperCard Pro */
} hardware_format_type_t;

/**
 * @brief Auto-detect hardware flux format from buffer
 * @param buffer File data buffer
 * @param size Buffer size in bytes
 * @return Detected format type
 */
static inline hardware_format_type_t hardware_detect_format(const uint8_t* buffer, size_t size)
{
    if (!buffer || size < 16) return HARDWARE_FORMAT_UNKNOWN;
    
    /* GWFLUX: "GWF\0" signature */
    if (size >= 4 && buffer[0] == 'G' && buffer[1] == 'W' && 
        buffer[2] == 'F' && buffer[3] == 0) {
        return HARDWARE_FORMAT_GWFLUX;
    }
    
    if (uft_kfs_detect(buffer, size)) {
        return HARDWARE_FORMAT_KFS;
    }
    
    /* SCP: "SCP" signature (check existing format) */
    /* Add SCP detection here if needed */
    
    return HARDWARE_FORMAT_UNKNOWN;
}

/**
 * @brief Get format name string
 * @param fmt Format type
 * @return Format name
 */
static inline const char* hardware_format_name(hardware_format_type_t fmt)
{
    switch (fmt) {
        case HARDWARE_FORMAT_GWFLUX: return "GWFLUX (Greaseweazle Flux)";
        case HARDWARE_FORMAT_KFS: return "KFS (KryoFlux Stream)";
        case HARDWARE_FORMAT_SCP: return "SCP (SuperCard Pro)";
        default: return "Unknown";
    }
}

/*============================================================================*
 * HARDWARE DEVICES
 *============================================================================*/

/**
 * @brief Common floppy disk reading hardware devices
 */
typedef struct {
    const char* name;
    const char* format;
    const char* price_range;
    const char* availability;
} hardware_device_t;

static const hardware_device_t HARDWARE_DEVICES[] = {
    { "Greaseweazle",   "GWFLUX", "$30",      "Widely available" },
    { "KryoFlux",       "KFS",    "$100+",    "Professional" },
    { "SuperCard Pro",  "SCP",    "$150+",    "Professional" },
    { "Applesauce FDC", "Various","$150+",    "Apple-focused" },
    { "FluxEngine",     "Various","DIY",      "Open hardware" },
    { NULL, NULL, NULL, NULL }
};

/*============================================================================*
 * FLUX FORMAT NOTES
 *============================================================================*/

/**
 * HARDWARE FLUX FORMATS - CRITICAL UNDERSTANDING:
 * 
 * These are TIER 0 formats - the foundation of preservation!
 * 
 *   - $30 USB device, community favorite
 *   - Open source firmware
 *   - Simple .gwf container format
 *   - Pure flux delta timings
 *   - Track/side separated
 * 
 *   - Professional preservation standard
 *   - Used by museums and archives
 *   - Stream-based format (trackNN.raw)
 *   - Variable timing resolution
 *   - Industry standard for archiving
 * 
 * SCP (SuperCard Pro):
 *   - High-end preservation hardware
 *   - Professional-grade captures
 *   - Track-based flux storage
 *   - Excellent for copy-protected disks
 * 
 * WHY THESE ARE CRITICAL:
 *   - Capture raw magnetic transitions
 *   - Platform-independent
 *   - Preserve copy-protection
 *   - Enable future format discovery
 *   - Museum-grade preservation
 *   - Can decode to ANY logical format
 * 
 * WORKFLOW:
 *   Hardware → FLUX → Pipeline → Logical Format
 *   
 *   Example:
 *     SCP → .scp → Custom decoder → Unknown format!
 * 
 * WITHOUT FLUX SUPPORT:
 *   ❌ Cannot preserve copy-protected disks
 *   ❌ Cannot discover unknown formats
 *   ❌ Cannot verify preservation quality
 *   ❌ Limited to known logical formats
 * 
 * WITH FLUX SUPPORT:
 *   ✅ Complete preservation capability
 *   ✅ Future-proof archiving
 *   ✅ Copy-protection preserved
 *   ✅ Museum-grade quality
 *   ✅ Format discovery possible
 */

#ifdef __cplusplus
}
#endif

#endif /* HARDWARE_FORMATS_H */
