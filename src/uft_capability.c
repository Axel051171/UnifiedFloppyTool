/**
 * @file uft_capability.c
 * @brief Runtime Capability Matrix Implementation
 * 
 * TICKET-007: Capability Matrix Runtime
 */

#include "uft/uft_capability.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Format Database
 * ═══════════════════════════════════════════════════════════════════════════════ */

static const uft_format_info_t format_db[] = {
    /* Amiga Formats */
    {UFT_FORMAT_ADF, "ADF", "Amiga Disk File", "adf",
     UFT_CAP_READ | UFT_CAP_WRITE | UFT_CAP_CONVERT_FROM | UFT_CAP_CONVERT_TO | 
     UFT_CAP_ANALYZE | UFT_CAP_VERIFY,
     80, 84, 2, 2, 11, 22, "linux,macos,windows", NULL, NULL, "1.0", "UFT", NULL},
    
    /* Commodore Formats */
    {UFT_FORMAT_D64, "D64", "Commodore 64 Disk Image", "d64",
     UFT_CAP_READ | UFT_CAP_WRITE | UFT_CAP_CONVERT_FROM | UFT_CAP_CONVERT_TO |
     UFT_CAP_ANALYZE | UFT_CAP_PROTECTION,
     35, 42, 1, 1, 17, 21, "linux,macos,windows", NULL, NULL, "1.0", "UFT", NULL},
    
    {UFT_FORMAT_G64, "G64", "Commodore 64 GCR Image", "g64",
     UFT_CAP_READ | UFT_CAP_WRITE | UFT_CAP_CONVERT_FROM | UFT_CAP_FLUX |
     UFT_CAP_PROTECTION | UFT_CAP_HALF_TRACKS,
     35, 84, 1, 2, 0, 0, "linux,macos,windows", NULL, NULL, "1.0", "UFT", NULL},
    
    /* Flux Formats */
    {UFT_FORMAT_SCP, "SCP", "SuperCard Pro Flux", "scp",
     UFT_CAP_READ | UFT_CAP_WRITE | UFT_CAP_CONVERT_FROM | UFT_CAP_FLUX |
     UFT_CAP_MULTI_REV | UFT_CAP_WEAK_BITS | UFT_CAP_PROTECTION | UFT_CAP_INDEX_SYNC,
     0, 255, 1, 2, 0, 0, "linux,macos,windows", NULL, NULL, "3.0", "SCP", NULL},
    
    {UFT_FORMAT_HFE, "HFE", "UFT HFE Format", "hfe",
     UFT_CAP_READ | UFT_CAP_WRITE | UFT_CAP_CONVERT_FROM | UFT_CAP_CONVERT_TO |
     UFT_CAP_FLUX,
     0, 255, 1, 2, 0, 0, "linux,macos,windows", NULL, NULL, "3.0", "HxC", NULL},
    
    {UFT_FORMAT_WOZ, "WOZ", "Apple II Flux Image", "woz",
     UFT_CAP_READ | UFT_CAP_WRITE | UFT_CAP_CONVERT_FROM | UFT_CAP_FLUX |
     UFT_CAP_MULTI_REV | UFT_CAP_WEAK_BITS | UFT_CAP_HALF_TRACKS,
     0, 80, 1, 2, 0, 0, "linux,macos,windows", NULL, NULL, "2.1", "Applesauce", NULL},
    
    {UFT_FORMAT_A2R, "A2R", "Applesauce Raw Flux", "a2r",
     UFT_CAP_READ | UFT_CAP_CONVERT_FROM | UFT_CAP_FLUX | UFT_CAP_MULTI_REV |
     UFT_CAP_WEAK_BITS | UFT_CAP_INDEX_SYNC,
     0, 80, 1, 2, 0, 0, "linux,macos,windows", NULL, NULL, "3.0", "Applesauce", NULL},
    
    {UFT_FORMAT_IPF, "IPF", "Interchangeable Preservation Format", "ipf",
     UFT_CAP_READ | UFT_CAP_CONVERT_FROM | UFT_CAP_FLUX | UFT_CAP_PROTECTION |
     UFT_CAP_WEAK_BITS,
     0, 255, 1, 2, 0, 0, "linux,macos,windows", NULL, NULL, "2.0", "SPS/CAPS", NULL},
    
    /* PC Formats */
    {UFT_FORMAT_IMG, "IMG", "Raw Sector Image", "img,ima,dsk",
     UFT_CAP_READ | UFT_CAP_WRITE | UFT_CAP_CONVERT_FROM | UFT_CAP_CONVERT_TO |
     UFT_CAP_ANALYZE | UFT_CAP_VERIFY,
     40, 82, 1, 2, 8, 36, "linux,macos,windows", NULL, NULL, "1.0", "UFT", NULL},
    
    {UFT_FORMAT_IMD, "IMD", "ImageDisk", "imd",
     UFT_CAP_READ | UFT_CAP_WRITE | UFT_CAP_CONVERT_FROM | UFT_CAP_CONVERT_TO |
     UFT_CAP_ANALYZE,
     0, 255, 1, 2, 0, 255, "linux,macos,windows", NULL, NULL, "1.18", "ImageDisk", NULL},
    
    {UFT_FORMAT_TD0, "TD0", "Teledisk", "td0",
     UFT_CAP_READ | UFT_CAP_CONVERT_FROM | UFT_CAP_ANALYZE,
     0, 255, 1, 2, 0, 255, "linux,macos,windows", NULL, NULL, "2.0", "Sydex", NULL},
    
    {UFT_FORMAT_DMK, "DMK", "David M. Keil TRS-80 Format", "dmk",
     UFT_CAP_READ | UFT_CAP_WRITE | UFT_CAP_CONVERT_FROM | UFT_CAP_FLUX,
     0, 255, 1, 2, 0, 0, "linux,macos,windows", NULL, NULL, "1.0", "UFT", NULL},
    
    /* Apple Formats */
    {UFT_FORMAT_NIB, "NIB", "Apple II Nibble Image", "nib",
     UFT_CAP_READ | UFT_CAP_WRITE | UFT_CAP_CONVERT_FROM | UFT_CAP_FLUX,
     35, 40, 1, 1, 0, 0, "linux,macos,windows", NULL, NULL, "1.0", "UFT", NULL},
    
    {UFT_FORMAT_DO, "DO", "Apple DOS Order", "do,dsk",
     UFT_CAP_READ | UFT_CAP_WRITE | UFT_CAP_CONVERT_FROM | UFT_CAP_CONVERT_TO,
     35, 40, 1, 1, 16, 16, "linux,macos,windows", NULL, NULL, "1.0", "UFT", NULL},
    
    {UFT_FORMAT_PO, "PO", "Apple ProDOS Order", "po",
     UFT_CAP_READ | UFT_CAP_WRITE | UFT_CAP_CONVERT_FROM | UFT_CAP_CONVERT_TO,
     35, 40, 1, 1, 16, 16, "linux,macos,windows", NULL, NULL, "1.0", "UFT", NULL},
    
    /* Atari Formats */
    {UFT_FORMAT_ATR, "ATR", "Atari 8-bit Disk Image", "atr",
     UFT_CAP_READ | UFT_CAP_WRITE | UFT_CAP_CONVERT_FROM | UFT_CAP_CONVERT_TO |
     UFT_CAP_ANALYZE,
     1, 80, 1, 2, 18, 26, "linux,macos,windows", NULL, NULL, "1.0", "UFT", NULL},
    
    {UFT_FORMAT_ATX, "ATX", "Atari Extended Disk Image", "atx",
     UFT_CAP_READ | UFT_CAP_CONVERT_FROM | UFT_CAP_PROTECTION | UFT_CAP_WEAK_BITS,
     1, 80, 1, 2, 0, 0, "linux,macos,windows", NULL, NULL, "1.0", "VAPI", NULL},
    
    {UFT_FORMAT_STX, "STX", "Atari ST Pasti Image", "stx",
     UFT_CAP_READ | UFT_CAP_CONVERT_FROM | UFT_CAP_FLUX | UFT_CAP_PROTECTION,
     0, 86, 1, 2, 0, 0, "linux,macos,windows", NULL, NULL, "1.0", "Pasti", NULL},
    
    /* Terminator */
    {UFT_FORMAT_UNKNOWN, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL}
};

#define FORMAT_COUNT (sizeof(format_db) / sizeof(format_db[0]) - 1)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Hardware Database
 * ═══════════════════════════════════════════════════════════════════════════════ */

static const uft_hardware_info_t hardware_db[] = {
     UFT_HW_CAP_READ | UFT_HW_CAP_WRITE | UFT_HW_CAP_FLUX_READ | UFT_HW_CAP_FLUX_WRITE |
     UFT_HW_CAP_MULTI_REV | UFT_HW_CAP_INDEX | UFT_HW_CAP_DENSITY | UFT_HW_CAP_SIDE_SEL |
     UFT_HW_CAP_HD | UFT_HW_CAP_PRECOMP,
     4000000, 84000000, 28, 2, "3.5\",5.25\",8\"",
     UFT_PLATFORM_FULL, UFT_PLATFORM_FULL, UFT_PLATFORM_FULL,
    
     UFT_HW_CAP_READ | UFT_HW_CAP_WRITE | UFT_HW_CAP_FLUX_READ | UFT_HW_CAP_FLUX_WRITE |
     UFT_HW_CAP_MULTI_REV | UFT_HW_CAP_INDEX | UFT_HW_CAP_DENSITY,
     1000000, 12000000, 83, 2, "3.5\",5.25\"",
     UFT_PLATFORM_FULL, UFT_PLATFORM_FULL, UFT_PLATFORM_FULL,
     "USB", NULL, "http://cowlark.com/fluxengine/", NULL},
    
    {UFT_HW_KRYOFLUX, "KryoFlux", "Professional USB floppy controller", "SPS",
     UFT_HW_CAP_READ | UFT_HW_CAP_FLUX_READ | UFT_HW_CAP_MULTI_REV | UFT_HW_CAP_INDEX |
     UFT_HW_CAP_DENSITY | UFT_HW_CAP_HD | UFT_HW_CAP_8INCH,
     1000000, 24000000, 41, 4, "3.5\",5.25\",8\"",
     UFT_PLATFORM_FULL, UFT_PLATFORM_PARTIAL, UFT_PLATFORM_FULL,
     "USB", "kryoflux", "https://kryoflux.com/", NULL},
    
    {UFT_HW_SUPERCARDPRO, "SuperCard Pro", "Professional flux controller", "Jim Drew",
     UFT_HW_CAP_READ | UFT_HW_CAP_WRITE | UFT_HW_CAP_FLUX_READ | UFT_HW_CAP_FLUX_WRITE |
     UFT_HW_CAP_MULTI_REV | UFT_HW_CAP_INDEX | UFT_HW_CAP_HD,
     1000000, 50000000, 25, 2, "3.5\",5.25\"",
     UFT_PLATFORM_FULL, UFT_PLATFORM_PARTIAL, UFT_PLATFORM_FULL,
     "USB", NULL, "https://www.cbmstuff.com/", NULL},
    
    {UFT_HW_FC5025, "FC5025", "Device Side Data USB controller", "Device Side Data",
     UFT_HW_CAP_READ | UFT_HW_CAP_WRITE | UFT_HW_CAP_DENSITY,
     0, 0, 0, 1, "5.25\"",
     UFT_PLATFORM_PARTIAL, UFT_PLATFORM_PARTIAL, UFT_PLATFORM_FULL,
     "USB", "fc5025", "http://www.deviceside.com/", NULL},
    
    {UFT_HW_XUM1541, "XUM1541", "Commodore disk drive interface", "Various",
     UFT_HW_CAP_READ | UFT_HW_CAP_WRITE,
     0, 0, 0, 4, "1541,1571,1581",
     UFT_PLATFORM_FULL, UFT_PLATFORM_PARTIAL, UFT_PLATFORM_PARTIAL,
     "USB", "opencbm", "https://github.com/CBM library/CBM library", NULL},
    
    /* Terminator */
    {UFT_HW_NONE, NULL, NULL, NULL, 0, 0, 0, 0, 0, NULL,
     UFT_PLATFORM_UNSUPPORTED, UFT_PLATFORM_UNSUPPORTED, UFT_PLATFORM_UNSUPPORTED,
     NULL, NULL, NULL, NULL}
};

#define HARDWARE_COUNT (sizeof(hardware_db) / sizeof(hardware_db[0]) - 1)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Compatibility Matrix
 * ═══════════════════════════════════════════════════════════════════════════════ */

static const uft_compat_entry_t compat_db[] = {
    {UFT_FORMAT_ADF, UFT_HW_GREASEWEAZLE, 
     UFT_CAP_READ | UFT_CAP_WRITE | UFT_CAP_FLUX | UFT_CAP_VERIFY, 100,
     "Full support", NULL},
    {UFT_FORMAT_D64, UFT_HW_GREASEWEAZLE,
     UFT_CAP_READ | UFT_CAP_WRITE | UFT_CAP_FLUX | UFT_CAP_PROTECTION, 95,
     "Excellent with 1541 drive profile", NULL},
    {UFT_FORMAT_SCP, UFT_HW_GREASEWEAZLE,
     UFT_CAP_READ | UFT_CAP_WRITE | UFT_CAP_FLUX | UFT_CAP_MULTI_REV, 100,
     "Native format", NULL},
     
    {UFT_FORMAT_ADF, UFT_HW_FLUXENGINE,
     UFT_CAP_READ | UFT_CAP_WRITE | UFT_CAP_FLUX, 95,
     "Good support", NULL},
    {UFT_FORMAT_D64, UFT_HW_FLUXENGINE,
     UFT_CAP_READ | UFT_CAP_WRITE | UFT_CAP_FLUX, 90,
     "Good support", "May need timing adjustments"},
     
    {UFT_FORMAT_IPF, UFT_HW_KRYOFLUX,
     UFT_CAP_READ | UFT_CAP_FLUX | UFT_CAP_PROTECTION, 100,
     "Best for preservation", "Write limited"},
    {UFT_FORMAT_SCP, UFT_HW_KRYOFLUX,
     UFT_CAP_READ | UFT_CAP_FLUX | UFT_CAP_MULTI_REV, 95,
     "Excellent read quality", "Write limited"},
     
    /* XUM1541 - C64 specific */
    {UFT_FORMAT_D64, UFT_HW_XUM1541,
     UFT_CAP_READ | UFT_CAP_WRITE, 100,
     "Native Commodore support", "Requires 1541/1571 drive"},
    {UFT_FORMAT_G64, UFT_HW_XUM1541,
     UFT_CAP_READ | UFT_CAP_WRITE | UFT_CAP_PROTECTION, 95,
     "GCR support via GCR tools", NULL},
    
    /* Terminator */
    {UFT_FORMAT_UNKNOWN, UFT_HW_NONE, 0, 0, NULL, NULL}
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Capability Query Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool uft_capability_check(uft_format_t format, uft_capability_t capability) {
    uint32_t caps = uft_capability_get(format);
    return (caps & capability) != 0;
}

uint32_t uft_capability_get(uft_format_t format) {
    for (int i = 0; format_db[i].name; i++) {
        if (format_db[i].format == format) {
            return format_db[i].capabilities;
        }
    }
    return 0;
}

uint32_t uft_hw_capability_get(uft_hardware_t hardware) {
    for (int i = 0; hardware_db[i].name; i++) {
        if (hardware_db[i].hardware == hardware) {
            return hardware_db[i].capabilities;
        }
    }
    return 0;
}

bool uft_capability_compatible(uft_format_t format, uft_hardware_t hardware,
                                uft_capability_result_t *result) {
    /* Check specific compatibility entry */
    for (int i = 0; compat_db[i].format != UFT_FORMAT_UNKNOWN; i++) {
        if (compat_db[i].format == format && compat_db[i].hardware == hardware) {
            if (result) {
                result->supported = true;
                result->capabilities = compat_db[i].capabilities;
                result->quality = compat_db[i].quality;
                result->message = compat_db[i].notes;
                result->suggestion = compat_db[i].limitations;
            }
            return true;
        }
    }
    
    /* Generic compatibility check */
    uint32_t fmt_caps = uft_capability_get(format);
    uint32_t hw_caps = uft_hw_capability_get(hardware);
    
    bool can_read = (fmt_caps & UFT_CAP_READ) && (hw_caps & UFT_HW_CAP_READ);
    bool can_write = (fmt_caps & UFT_CAP_WRITE) && (hw_caps & UFT_HW_CAP_WRITE);
    
    if (result) {
        result->supported = can_read || can_write;
        result->capabilities = 0;
        if (can_read) result->capabilities |= UFT_CAP_READ;
        if (can_write) result->capabilities |= UFT_CAP_WRITE;
        result->quality = 50;  /* Generic compatibility */
        result->message = "Generic compatibility";
        result->suggestion = "Check specific format requirements";
    }
    
    return can_read || can_write;
}

uft_capability_result_t uft_capability_query(uft_format_t format,
                                              uft_hardware_t hardware,
                                              uft_capability_t operation) {
    uft_capability_result_t result = {0};
    
    if (hardware == UFT_HW_NONE) {
        /* Format-only query */
        uint32_t caps = uft_capability_get(format);
        result.supported = (caps & operation) != 0;
        result.capabilities = caps;
        result.quality = result.supported ? 100 : 0;
        result.message = result.supported ? "Format supports operation" : "Format does not support operation";
    } else {
        uft_capability_compatible(format, hardware, &result);
        result.supported = result.supported && (result.capabilities & operation);
    }
    
    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Format Information Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

const uft_format_info_t *uft_format_get_info(uft_format_t format) {
    for (int i = 0; format_db[i].name; i++) {
        if (format_db[i].format == format) {
            return &format_db[i];
        }
    }
    return NULL;
}

uft_format_t uft_format_by_name(const char *name) {
    if (!name) return UFT_FORMAT_UNKNOWN;
    
    for (int i = 0; format_db[i].name; i++) {
        if (strcasecmp(format_db[i].name, name) == 0) {
            return format_db[i].format;
        }
    }
    return UFT_FORMAT_UNKNOWN;
}

uft_format_t uft_format_by_extension(const char *extension) {
    if (!extension) return UFT_FORMAT_UNKNOWN;
    
    /* Skip leading dot */
    if (*extension == '.') extension++;
    
    for (int i = 0; format_db[i].name; i++) {
        if (!format_db[i].extensions) continue;
        
        /* Check each extension in comma-separated list */
        char exts[128];
        strncpy(exts, format_db[i].extensions, sizeof(exts) - 1);
        
        char *token = strtok(exts, ",");
        while (token) {
            if (strcasecmp(token, extension) == 0) {
                return format_db[i].format;
            }
            token = strtok(NULL, ",");
        }
    }
    return UFT_FORMAT_UNKNOWN;
}

const uft_format_t *uft_format_list_all(int *count) {
    static uft_format_t formats[64];
    static int cached_count = 0;
    
    if (cached_count == 0) {
        for (int i = 0; format_db[i].name && cached_count < 63; i++) {
            formats[cached_count++] = format_db[i].format;
        }
    }
    
    *count = cached_count;
    return formats;
}

uft_format_t *uft_format_list_by_capability(uft_capability_t capability, int *count) {
    uft_format_t *list = malloc(FORMAT_COUNT * sizeof(uft_format_t));
    if (!list) {
        *count = 0;
        return NULL;
    }
    
    *count = 0;
    for (int i = 0; format_db[i].name; i++) {
        if (format_db[i].capabilities & capability) {
            list[(*count)++] = format_db[i].format;
        }
    }
    
    return list;
}

uft_format_t *uft_format_list_by_hardware(uft_hardware_t hardware, int *count) {
    uft_format_t *list = malloc(FORMAT_COUNT * sizeof(uft_format_t));
    if (!list) {
        *count = 0;
        return NULL;
    }
    
    *count = 0;
    for (int i = 0; format_db[i].name; i++) {
        uft_capability_result_t result;
        if (uft_capability_compatible(format_db[i].format, hardware, &result)) {
            list[(*count)++] = format_db[i].format;
        }
    }
    
    return list;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Hardware Information Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

const uft_hardware_info_t *uft_hardware_get_info(uft_hardware_t hardware) {
    for (int i = 0; hardware_db[i].name; i++) {
        if (hardware_db[i].hardware == hardware) {
            return &hardware_db[i];
        }
    }
    return NULL;
}

uft_hardware_t uft_hardware_by_name(const char *name) {
    if (!name) return UFT_HW_NONE;
    
    for (int i = 0; hardware_db[i].name; i++) {
        if (strcasecmp(hardware_db[i].name, name) == 0) {
            return hardware_db[i].hardware;
        }
    }
    return UFT_HW_NONE;
}

const uft_hardware_t *uft_hardware_list_all(int *count) {
    static uft_hardware_t hardware[16];
    static int cached_count = 0;
    
    if (cached_count == 0) {
        for (int i = 0; hardware_db[i].name && cached_count < 15; i++) {
            hardware[cached_count++] = hardware_db[i].hardware;
        }
    }
    
    *count = cached_count;
    return hardware;
}

uft_hardware_t *uft_hardware_list_by_capability(uft_hw_capability_t capability, int *count) {
    uft_hardware_t *list = malloc(HARDWARE_COUNT * sizeof(uft_hardware_t));
    if (!list) {
        *count = 0;
        return NULL;
    }
    
    *count = 0;
    for (int i = 0; hardware_db[i].name; i++) {
        if (hardware_db[i].capabilities & capability) {
            list[(*count)++] = hardware_db[i].hardware;
        }
    }
    
    return list;
}

uft_platform_support_t uft_hardware_platform_support(uft_hardware_t hardware) {
    const uft_hardware_info_t *info = uft_hardware_get_info(hardware);
    if (!info) return UFT_PLATFORM_UNSUPPORTED;
    
#if defined(__linux__)
    return info->linux_support;
#elif defined(__APPLE__)
    return info->macos_support;
#elif defined(_WIN32)
    return info->windows_support;
#else
    return UFT_PLATFORM_UNSUPPORTED;
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Compatibility Matrix Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

const uft_compat_entry_t *uft_compat_get(uft_format_t format, uft_hardware_t hardware) {
    for (int i = 0; compat_db[i].format != UFT_FORMAT_UNKNOWN; i++) {
        if (compat_db[i].format == format && compat_db[i].hardware == hardware) {
            return &compat_db[i];
        }
    }
    return NULL;
}

uft_hardware_t uft_compat_best_hardware(uft_format_t format, uft_capability_t operation) {
    uft_hardware_t best = UFT_HW_NONE;
    int best_quality = 0;
    
    for (int i = 0; compat_db[i].format != UFT_FORMAT_UNKNOWN; i++) {
        if (compat_db[i].format == format && 
            (compat_db[i].capabilities & operation) &&
            compat_db[i].quality > best_quality) {
            best = compat_db[i].hardware;
            best_quality = compat_db[i].quality;
        }
    }
    
    return best;
}

uft_format_t uft_compat_best_target(uft_format_t source, uint32_t preserve_caps) {
    uint32_t source_caps = uft_capability_get(source);
    uft_format_t best = UFT_FORMAT_UNKNOWN;
    int best_score = 0;
    
    for (int i = 0; format_db[i].name; i++) {
        if (format_db[i].format == source) continue;
        if (!(format_db[i].capabilities & UFT_CAP_CONVERT_TO)) continue;
        
        /* Score based on preserved capabilities */
        int score = 0;
        uint32_t target_caps = format_db[i].capabilities;
        
        if (preserve_caps & UFT_CAP_FLUX) {
            if (target_caps & UFT_CAP_FLUX) score += 10;
        }
        if (preserve_caps & UFT_CAP_PROTECTION) {
            if (target_caps & UFT_CAP_PROTECTION) score += 8;
        }
        if (preserve_caps & UFT_CAP_WEAK_BITS) {
            if (target_caps & UFT_CAP_WEAK_BITS) score += 5;
        }
        
        if (score > best_score) {
            best = format_db[i].format;
            best_score = score;
        }
    }
    
    return best;
}

int uft_compat_conversion_path(uft_format_t source, uft_format_t target,
                                uft_format_t *path, int max_steps) {
    /* Direct conversion? */
    uint32_t src_caps = uft_capability_get(source);
    uint32_t tgt_caps = uft_capability_get(target);
    
    if ((src_caps & UFT_CAP_CONVERT_FROM) && (tgt_caps & UFT_CAP_CONVERT_TO)) {
        if (path && max_steps >= 2) {
            path[0] = source;
            path[1] = target;
        }
        return 2;
    }
    
    /* Try via intermediate format (SCP is universal) */
    if (max_steps >= 3 && source != UFT_FORMAT_SCP && target != UFT_FORMAT_SCP) {
        uint32_t scp_caps = uft_capability_get(UFT_FORMAT_SCP);
        if ((src_caps & UFT_CAP_CONVERT_FROM) && (scp_caps & UFT_CAP_CONVERT_TO) &&
            (scp_caps & UFT_CAP_CONVERT_FROM) && (tgt_caps & UFT_CAP_CONVERT_TO)) {
            if (path) {
                path[0] = source;
                path[1] = UFT_FORMAT_SCP;
                path[2] = target;
            }
            return 3;
        }
    }
    
    return -1;  /* No path found */
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Discovery API Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

char *uft_capability_discover(uft_hardware_t detected_hw, uft_format_t source_format) {
    size_t size = 4096;
    char *json = malloc(size);
    if (!json) return NULL;
    
    int pos = 0;
    pos += snprintf(json + pos, size - pos, "{\n");
    
    /* Hardware info */
    if (detected_hw != UFT_HW_NONE) {
        const uft_hardware_info_t *hw = uft_hardware_get_info(detected_hw);
        if (hw) {
            pos += snprintf(json + pos, size - pos,
                "  \"hardware\": {\n"
                "    \"name\": \"%s\",\n"
                "    \"description\": \"%s\",\n"
                "    \"can_read\": %s,\n"
                "    \"can_write\": %s,\n"
                "    \"can_flux\": %s\n"
                "  },\n",
                hw->name, hw->description,
                (hw->capabilities & UFT_HW_CAP_READ) ? "true" : "false",
                (hw->capabilities & UFT_HW_CAP_WRITE) ? "true" : "false",
                (hw->capabilities & UFT_HW_CAP_FLUX_READ) ? "true" : "false");
        }
    }
    
    /* Available operations */
    pos += snprintf(json + pos, size - pos, "  \"operations\": {\n");
    
    if (source_format != UFT_FORMAT_UNKNOWN) {
        uint32_t caps = uft_capability_get(source_format);
        pos += snprintf(json + pos, size - pos,
            "    \"read\": %s,\n"
            "    \"write\": %s,\n"
            "    \"convert\": %s,\n"
            "    \"analyze\": %s,\n"
            "    \"recover\": %s,\n"
            "    \"verify\": %s\n",
            (caps & UFT_CAP_READ) ? "true" : "false",
            (caps & UFT_CAP_WRITE) ? "true" : "false",
            (caps & UFT_CAP_CONVERT_FROM) ? "true" : "false",
            (caps & UFT_CAP_ANALYZE) ? "true" : "false",
            (caps & UFT_CAP_RECOVER) ? "true" : "false",
            (caps & UFT_CAP_VERIFY) ? "true" : "false");
    } else {
        pos += snprintf(json + pos, size - pos,
            "    \"read\": true,\n"
            "    \"analyze\": true\n");
    }
    
    pos += snprintf(json + pos, size - pos, "  }\n}\n");
    
    return json;
}

char *uft_capability_suggest(uint32_t current_caps, uint32_t desired_caps) {
    size_t size = 2048;
    char *json = malloc(size);
    if (!json) return NULL;
    
    int pos = 0;
    pos += snprintf(json + pos, size - pos, "{\n  \"suggestions\": [\n");
    
    uint32_t missing = desired_caps & ~current_caps;
    bool first = true;
    
    if (missing & UFT_CAP_FLUX) {
        if (!first) pos += snprintf(json + pos, size - pos, ",\n");
        pos += snprintf(json + pos, size - pos,
            "    {\"need\": \"flux\", \"suggestion\": \"Use Greaseweazle or KryoFlux for flux capture\"}");
        first = false;
    }
    
    if (missing & UFT_CAP_WRITE) {
        if (!first) pos += snprintf(json + pos, size - pos, ",\n");
        pos += snprintf(json + pos, size - pos,
            "    {\"need\": \"write\", \"suggestion\": \"Use Greaseweazle or FluxEngine for write support\"}");
        first = false;
    }
    
    if (missing & UFT_CAP_PROTECTION) {
        if (!first) pos += snprintf(json + pos, size - pos, ",\n");
        pos += snprintf(json + pos, size - pos,
            "    {\"need\": \"protection\", \"suggestion\": \"Use flux formats (SCP, IPF) for copy protection preservation\"}");
        first = false;
    }
    
    pos += snprintf(json + pos, size - pos, "\n  ]\n}\n");
    
    return json;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Export Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

char *uft_capability_export_json(bool pretty) {
    size_t size = 32768;
    char *json = malloc(size);
    if (!json) return NULL;
    
    const char *nl = pretty ? "\n" : "";
    const char *sp = pretty ? "  " : "";
    
    int pos = 0;
    pos += snprintf(json + pos, size - pos, "{%s", nl);
    
    /* Formats */
    pos += snprintf(json + pos, size - pos, "%s\"formats\": [%s", sp, nl);
    for (int i = 0; format_db[i].name && pos < (int)size - 512; i++) {
        const uft_format_info_t *f = &format_db[i];
        pos += snprintf(json + pos, size - pos,
            "%s%s{\"id\": %d, \"name\": \"%s\", \"description\": \"%s\", "
            "\"extensions\": \"%s\", \"capabilities\": %u}%s%s",
            sp, sp, f->format, f->name, f->description,
            f->extensions ? f->extensions : "", f->capabilities,
            format_db[i+1].name ? "," : "", nl);
    }
    pos += snprintf(json + pos, size - pos, "%s],%s", sp, nl);
    
    /* Hardware */
    pos += snprintf(json + pos, size - pos, "%s\"hardware\": [%s", sp, nl);
    for (int i = 0; hardware_db[i].name && pos < (int)size - 512; i++) {
        const uft_hardware_info_t *h = &hardware_db[i];
        pos += snprintf(json + pos, size - pos,
            "%s%s{\"id\": %d, \"name\": \"%s\", \"vendor\": \"%s\", "
            "\"capabilities\": %u}%s%s",
            sp, sp, h->hardware, h->name, h->vendor ? h->vendor : "",
            h->capabilities,
            hardware_db[i+1].name ? "," : "", nl);
    }
    pos += snprintf(json + pos, size - pos, "%s]%s", sp, nl);
    
    pos += snprintf(json + pos, size - pos, "}%s", nl);
    
    return json;
}

char *uft_capability_export_markdown(void) {
    size_t size = 16384;
    char *md = malloc(size);
    if (!md) return NULL;
    
    int pos = 0;
    
    pos += snprintf(md + pos, size - pos, "# UFT Capability Matrix\n\n");
    
    /* Format table */
    pos += snprintf(md + pos, size - pos, "## Supported Formats\n\n");
    pos += snprintf(md + pos, size - pos, 
        "| Format | Extensions | Read | Write | Flux | Protection |\n"
        "|--------|------------|------|-------|------|------------|\n");
    
    for (int i = 0; format_db[i].name && pos < (int)size - 256; i++) {
        const uft_format_info_t *f = &format_db[i];
        pos += snprintf(md + pos, size - pos,
            "| %s | %s | %s | %s | %s | %s |\n",
            f->name,
            f->extensions ? f->extensions : "-",
            (f->capabilities & UFT_CAP_READ) ? "✅" : "❌",
            (f->capabilities & UFT_CAP_WRITE) ? "✅" : "❌",
            (f->capabilities & UFT_CAP_FLUX) ? "✅" : "❌",
            (f->capabilities & UFT_CAP_PROTECTION) ? "✅" : "❌");
    }
    
    /* Hardware table */
    pos += snprintf(md + pos, size - pos, "\n## Supported Hardware\n\n");
    pos += snprintf(md + pos, size - pos,
        "| Hardware | Read | Write | Flux | Multi-Rev | Linux | macOS | Windows |\n"
        "|----------|------|-------|------|-----------|-------|-------|--------|\n");
    
    for (int i = 0; hardware_db[i].name && pos < (int)size - 256; i++) {
        const uft_hardware_info_t *h = &hardware_db[i];
        pos += snprintf(md + pos, size - pos,
            "| %s | %s | %s | %s | %s | %s | %s | %s |\n",
            h->name,
            (h->capabilities & UFT_HW_CAP_READ) ? "✅" : "❌",
            (h->capabilities & UFT_HW_CAP_WRITE) ? "✅" : "❌",
            (h->capabilities & UFT_HW_CAP_FLUX_READ) ? "✅" : "❌",
            (h->capabilities & UFT_HW_CAP_MULTI_REV) ? "✅" : "❌",
            h->linux_support == UFT_PLATFORM_FULL ? "✅" : "⚠️",
            h->macos_support == UFT_PLATFORM_FULL ? "✅" : "⚠️",
            h->windows_support == UFT_PLATFORM_FULL ? "✅" : "⚠️");
    }
    
    return md;
}

char *uft_capability_export_html(void) {
    char *md = uft_capability_export_markdown();
    if (!md) return NULL;
    
    /* Simple conversion - in real impl use proper Markdown parser */
    size_t size = strlen(md) * 2 + 1024;
    char *html = malloc(size);
    if (!html) {
        free(md);
        return NULL;
    }
    
    snprintf(html, size,
        "<!DOCTYPE html>\n<html>\n<head>\n"
        "<title>UFT Capability Matrix</title>\n"
        "<style>table{border-collapse:collapse}td,th{border:1px solid #ddd;padding:8px}</style>\n"
        "</head>\n<body>\n<pre>%s</pre>\n</body>\n</html>", md);
    
    free(md);
    return html;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

const char *uft_capability_name(uft_capability_t cap) {
    switch (cap) {
        case UFT_CAP_READ:         return "READ";
        case UFT_CAP_WRITE:        return "WRITE";
        case UFT_CAP_CONVERT_FROM: return "CONVERT_FROM";
        case UFT_CAP_CONVERT_TO:   return "CONVERT_TO";
        case UFT_CAP_ANALYZE:      return "ANALYZE";
        case UFT_CAP_RECOVER:      return "RECOVER";
        case UFT_CAP_VERIFY:       return "VERIFY";
        case UFT_CAP_FLUX:         return "FLUX";
        case UFT_CAP_PROTECTION:   return "PROTECTION";
        case UFT_CAP_MULTI_REV:    return "MULTI_REV";
        case UFT_CAP_WEAK_BITS:    return "WEAK_BITS";
        case UFT_CAP_HALF_TRACKS:  return "HALF_TRACKS";
        case UFT_CAP_VARIABLE_RPM: return "VARIABLE_RPM";
        case UFT_CAP_INDEX_SYNC:   return "INDEX_SYNC";
        default:                   return "UNKNOWN";
    }
}

const char *uft_hw_capability_name(uft_hw_capability_t cap) {
    switch (cap) {
        case UFT_HW_CAP_READ:       return "READ";
        case UFT_HW_CAP_WRITE:      return "WRITE";
        case UFT_HW_CAP_FLUX_READ:  return "FLUX_READ";
        case UFT_HW_CAP_FLUX_WRITE: return "FLUX_WRITE";
        case UFT_HW_CAP_MULTI_REV:  return "MULTI_REV";
        case UFT_HW_CAP_INDEX:      return "INDEX";
        case UFT_HW_CAP_DENSITY:    return "DENSITY";
        case UFT_HW_CAP_SIDE_SEL:   return "SIDE_SEL";
        case UFT_HW_CAP_MOTOR_CTRL: return "MOTOR_CTRL";
        case UFT_HW_CAP_ERASE:      return "ERASE";
        case UFT_HW_CAP_PRECOMP:    return "PRECOMP";
        case UFT_HW_CAP_HD:         return "HD";
        case UFT_HW_CAP_ED:         return "ED";
        case UFT_HW_CAP_8INCH:      return "8INCH";
        default:                    return "UNKNOWN";
    }
}

const char *uft_platform_support_name(uft_platform_support_t level) {
    switch (level) {
        case UFT_PLATFORM_FULL:         return "Full";
        case UFT_PLATFORM_PARTIAL:      return "Partial";
        case UFT_PLATFORM_EXPERIMENTAL: return "Experimental";
        case UFT_PLATFORM_UNSUPPORTED:  return "Unsupported";
        default:                        return "Unknown";
    }
}

char *uft_capability_flags_string(uint32_t caps) {
    char *str = malloc(256);
    if (!str) return NULL;
    
    str[0] = '\0';
    for (int i = 0; i < 14; i++) {
        if (caps & (1 << i)) {
            if (str[0]) strncat(str, "|", 255 - strlen(str));
            strncat(str, uft_capability_name(1 << i), 255 - strlen(str));
        }
    }
    
    return str;
}

uint32_t uft_capability_flags_parse(const char *str) {
    if (!str) return 0;
    
    uint32_t caps = 0;
    char buf[256];
    strncpy(buf, str, sizeof(buf) - 1);
    
    char *token = strtok(buf, "|,");
    while (token) {
        for (int i = 0; i < 14; i++) {
            if (strcasecmp(token, uft_capability_name(1 << i)) == 0) {
                caps |= (1 << i);
                break;
            }
        }
        token = strtok(NULL, "|,");
    }
    
    return caps;
}

void uft_capability_print_summary(void) {
    int fmt_count, hw_count;
    uft_format_list_all(&fmt_count);
    uft_hardware_list_all(&hw_count);
    
    printf("UFT Capability Summary\n");
    printf("======================\n");
    printf("Formats:  %d\n", fmt_count);
    printf("Hardware: %d\n", hw_count);
    printf("\n");
    
    char *md = uft_capability_export_markdown();
    if (md) {
        printf("%s", md);
        free(md);
    }
}

void uft_format_print_info(uft_format_t format) {
    const uft_format_info_t *info = uft_format_get_info(format);
    if (!info) {
        printf("Unknown format: %d\n", format);
        return;
    }
    
    char *caps = uft_capability_flags_string(info->capabilities);
    
    printf("Format: %s\n", info->name);
    printf("  Description: %s\n", info->description);
    printf("  Extensions:  %s\n", info->extensions ? info->extensions : "-");
    printf("  Capabilities: %s\n", caps ? caps : "-");
    printf("  Geometry: %d-%d cyl, %d-%d heads, %d-%d sectors\n",
           info->min_cylinders, info->max_cylinders,
           info->min_heads, info->max_heads,
           info->min_sectors, info->max_sectors);
    
    free(caps);
}

void uft_hardware_print_info(uft_hardware_t hardware) {
    const uft_hardware_info_t *info = uft_hardware_get_info(hardware);
    if (!info) {
        printf("Unknown hardware: %d\n", hardware);
        return;
    }
    
    printf("Hardware: %s\n", info->name);
    printf("  Vendor: %s\n", info->vendor ? info->vendor : "-");
    printf("  Description: %s\n", info->description);
    printf("  Connection: %s\n", info->connection ? info->connection : "-");
    printf("  Sample Rate: %u - %u Hz\n", info->min_sample_rate, info->max_sample_rate);
    printf("  Platform Support:\n");
    printf("    Linux:   %s\n", uft_platform_support_name(info->linux_support));
    printf("    macOS:   %s\n", uft_platform_support_name(info->macos_support));
    printf("    Windows: %s\n", uft_platform_support_name(info->windows_support));
}
