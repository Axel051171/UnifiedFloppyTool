/**
 * @file uft_pc_protection.c
 * @brief PC Copy Protection Detection Implementation
 * 
 * TICKET-008: PC Protection Suite
 * Full implementation of SafeDisc, SecuROM, StarForce and DOS-era detection
 * 
 * @version 5.2.0
 * @date 2026-01-04
 */

#include "uft/uft_pc_protection.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Signature Database - SafeDisc
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* SafeDisc signature patterns */
static const uint8_t SAFEDISC_STXT[] = {
    0x42, 0x6F, 0x47, 0x5F  /* "BoG_" - signature in STXT files */
};

static const uint8_t SAFEDISC_CLCD[] = {
    0x43, 0x4C, 0x43, 0x44  /* "CLCD" header */
};

static const uint8_t SAFEDISC_DPLAYERX[] = {
    0x64, 0x70, 0x6C, 0x61, 0x79, 0x65, 0x72, 0x78  /* "dplayerx" */
};

static const uint8_t SAFEDISC_CLOKSPL[] = {
    0x63, 0x6C, 0x6F, 0x6B, 0x73, 0x70, 0x6C  /* "clokspl" */
};

/* SafeDisc version signatures */
static const uint8_t SAFEDISC_V1_SIG[] = {
    0x00, 0x00, 0x00, 0x01, 0x53, 0x54, 0x58, 0x54  /* Version 1.x marker */
};

static const uint8_t SAFEDISC_V2_SIG[] = {
    0x00, 0x00, 0x00, 0x02, 0x53, 0x54, 0x58, 0x54  /* Version 2.x marker */
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Signature Database - SecuROM
 * ═══════════════════════════════════════════════════════════════════════════════ */

static const uint8_t SECUROM_SECCDRV[] = {
    0x73, 0x65, 0x63, 0x63, 0x64, 0x72, 0x76  /* "seccdrv" */
};

static const uint8_t SECUROM_CMS16[] = {
    0x43, 0x4D, 0x53, 0x31, 0x36  /* "CMS16" */
};

static const uint8_t SECUROM_CMS32[] = {
    0x43, 0x4D, 0x53, 0x5F, 0x33, 0x32  /* "CMS_32" */
};

static const uint8_t SECUROM_PA_SIG[] = {
    0x53, 0x65, 0x63, 0x75, 0x52, 0x4F, 0x4D, 0x20,  /* "SecuROM " */
    0x50, 0x41                                        /* "PA" */
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Signature Database - StarForce
 * ═══════════════════════════════════════════════════════════════════════════════ */

static const uint8_t STARFORCE_PROTECT[] = {
    0x70, 0x72, 0x6F, 0x74, 0x65, 0x63, 0x74  /* "protect" driver pattern */
};

static const uint8_t STARFORCE_SFDRV[] = {
    0x73, 0x66, 0x64, 0x72, 0x76  /* "sfdrv" */
};

static const uint8_t STARFORCE_HEADER[] = {
    0x53, 0x54, 0x41, 0x52  /* "STAR" header */
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Signature Database - DOS/Floppy Era
 * ═══════════════════════════════════════════════════════════════════════════════ */

static const uint8_t PROLOCK_SIG[] = {
    0x50, 0x52, 0x4F, 0x4C, 0x4F, 0x43, 0x4B  /* "PROLOCK" */
};

static const uint8_t SOFTGUARD_SIG[] = {
    0x53, 0x47, 0x55, 0x41, 0x52, 0x44  /* "SGUARD" */
};

static const uint8_t COPYLOCK_PC_SIG[] = {
    0x43, 0x4F, 0x50, 0x59, 0x4C, 0x4F, 0x43, 0x4B  /* "COPYLOCK" */
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Master Signature Table
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const char          *name;
    uft_pc_protection_t type;
    const uint8_t       *pattern;
    size_t              length;
    int                 confidence;
    const char          *description;
} sig_entry_t;

static const sig_entry_t signature_db[] = {
    /* SafeDisc */
    {"SafeDisc BoG", UFT_PCPROT_SAFEDISC_1, SAFEDISC_STXT, 4, 75, "SafeDisc BoG signature"},
    {"SafeDisc CLCD", UFT_PCPROT_SAFEDISC_1, SAFEDISC_CLCD, 4, 70, "SafeDisc CLCD header"},
    {"SafeDisc dplayerx", UFT_PCPROT_SAFEDISC_2, SAFEDISC_DPLAYERX, 8, 85, "SafeDisc dplayerx.dll"},
    {"SafeDisc clokspl", UFT_PCPROT_SAFEDISC_2, SAFEDISC_CLOKSPL, 7, 80, "SafeDisc clock splice"},
    {"SafeDisc v1", UFT_PCPROT_SAFEDISC_1, SAFEDISC_V1_SIG, 8, 90, "SafeDisc 1.x marker"},
    {"SafeDisc v2", UFT_PCPROT_SAFEDISC_2, SAFEDISC_V2_SIG, 8, 90, "SafeDisc 2.x marker"},
    
    /* SecuROM */
    {"SecuROM seccdrv", UFT_PCPROT_SECUROM_4, SECUROM_SECCDRV, 7, 85, "SecuROM driver"},
    {"SecuROM CMS16", UFT_PCPROT_SECUROM_1, SECUROM_CMS16, 5, 80, "SecuROM 16-bit"},
    {"SecuROM CMS32", UFT_PCPROT_SECUROM_4, SECUROM_CMS32, 6, 80, "SecuROM 32-bit"},
    {"SecuROM PA", UFT_PCPROT_SECUROM_PA, SECUROM_PA_SIG, 10, 95, "SecuROM Product Activation"},
    
    /* StarForce */
    {"StarForce protect", UFT_PCPROT_STARFORCE_3, STARFORCE_PROTECT, 7, 75, "StarForce driver"},
    {"StarForce sfdrv", UFT_PCPROT_STARFORCE_3, STARFORCE_SFDRV, 5, 80, "StarForce sfdrv"},
    {"StarForce header", UFT_PCPROT_STARFORCE_1, STARFORCE_HEADER, 4, 60, "StarForce header"},
    
    /* DOS era */
    {"ProLock", UFT_PCPROT_PROLOCK, PROLOCK_SIG, 7, 90, "ProLock protection"},
    {"SoftGuard", UFT_PCPROT_SOFTGUARD, SOFTGUARD_SIG, 6, 85, "SoftGuard protection"},
    {"CopyLock PC", UFT_PCPROT_COPYLOCK_PC, COPYLOCK_PC_SIG, 8, 90, "CopyLock PC version"},
    
    /* Terminator */
    {NULL, UFT_PCPROT_UNKNOWN, NULL, 0, 0, NULL}
};

#define SIGNATURE_COUNT (sizeof(signature_db) / sizeof(signature_db[0]) - 1)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Geometry Signatures for Floppy Protections
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const char          *name;
    uft_pc_protection_t type;
    int                 track;
    int                 expected_sectors;
    bool                has_weak_bits;
    bool                has_crc_errors;
    int                 confidence;
    const char          *description;
} geo_sig_entry_t;

static const geo_sig_entry_t geometry_sigs[] = {
    /* Weak sector protections */
    {"Weak Track 0", UFT_PCPROT_WEAK_SECTOR, 0, -1, true, false, 70, "Weak bits on track 0"},
    {"Weak Track 6", UFT_PCPROT_WEAK_SECTOR, 6, -1, true, false, 80, "Common protection track"},
    {"Weak Track 39", UFT_PCPROT_WEAK_SECTOR, 39, -1, true, false, 85, "End-of-disk protection"},
    
    /* Extra track protections */
    {"Track 80", UFT_PCPROT_EXTRA_TRACK, 80, -1, false, false, 90, "Extra track beyond 80"},
    {"Track 81", UFT_PCPROT_EXTRA_TRACK, 81, -1, false, false, 90, "Extra track 81"},
    {"Track 82", UFT_PCPROT_EXTRA_TRACK, 82, -1, false, false, 90, "Extra track 82"},
    {"Track 83", UFT_PCPROT_EXTRA_TRACK, 83, -1, false, false, 90, "Extra track 83"},
    
    /* Non-standard sector counts */
    {"10 Sectors", UFT_PCPROT_LONG_TRACK, -1, 10, false, false, 60, "10 sectors per track"},
    {"11 Sectors", UFT_PCPROT_LONG_TRACK, -1, 11, false, false, 70, "11 sectors (Amiga-like)"},
    {"8 Sectors", UFT_PCPROT_SHORT_TRACK, -1, 8, false, false, 50, "8 sectors per track"},
    
    /* CRC error protections */
    {"CRC Track 6", UFT_PCPROT_CRC_ERROR, 6, -1, false, true, 80, "Intentional CRC on track 6"},
    {"CRC Track 39", UFT_PCPROT_CRC_ERROR, 39, -1, false, true, 85, "Intentional CRC on track 39"},
    
    /* Terminator */
    {NULL, UFT_PCPROT_UNKNOWN, -1, -1, false, false, 0, NULL}
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uint64_t get_time_ms(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

static char *strdup_safe(const char *str) {
    return str ? strdup(str) : NULL;
}

static bool memmem_search(const uint8_t *haystack, size_t haystack_len,
                          const uint8_t *needle, size_t needle_len,
                          size_t *offset_out) {
    if (needle_len > haystack_len) return false;
    
    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        if (memcmp(haystack + i, needle, needle_len) == 0) {
            if (offset_out) *offset_out = i;
            return true;
        }
    }
    return false;
}

static uft_pc_scan_result_t *result_create(void) {
    uft_pc_scan_result_t *result = calloc(1, sizeof(uft_pc_scan_result_t));
    if (!result) return NULL;
    
    result->capacity = 32;
    result->detections = calloc(result->capacity, sizeof(uft_pc_detection_t));
    
    return result;
}

static void result_add(uft_pc_scan_result_t *result, const uft_pc_detection_t *det) {
    if (!result || !det) return;
    
    if (result->count >= result->capacity) {
        result->capacity *= 2;
        result->detections = realloc(result->detections,
                                      result->capacity * sizeof(uft_pc_detection_t));
    }
    
    result->detections[result->count] = *det;
    result->detections[result->count].version = strdup_safe(det->version);
    result->detections[result->count].details = strdup_safe(det->details);
    result->detections[result->count].filename = strdup_safe(det->filename);
    result->count++;
    
    /* Update primary if higher confidence */
    if (det->confidence > result->primary_confidence) {
        result->primary = det->type;
        result->primary_confidence = det->confidence;
    }
    
    result->has_protection = true;
    if (det->confidence >= 75) {
        result->is_protected = true;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Signature Scanning
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void scan_signatures(const uint8_t *data, size_t size,
                            uft_pc_scan_result_t *result,
                            const uft_pc_scan_options_t *opts) {
    for (int i = 0; signature_db[i].name; i++) {
        const sig_entry_t *sig = &signature_db[i];
        
        /* Filter by protection type */
        if (opts) {
            if (!opts->check_safedisc && 
                (sig->type >= UFT_PCPROT_SAFEDISC_1 && sig->type <= UFT_PCPROT_SAFEDISC_4)) {
                continue;
            }
            if (!opts->check_securom &&
                (sig->type >= UFT_PCPROT_SECUROM_1 && sig->type <= UFT_PCPROT_SECUROM_PA)) {
                continue;
            }
            if (!opts->check_starforce &&
                (sig->type >= UFT_PCPROT_STARFORCE_1 && sig->type <= UFT_PCPROT_STARFORCE_3)) {
                continue;
            }
        }
        
        size_t offset;
        if (memmem_search(data, size, sig->pattern, sig->length, &offset)) {
            uft_pc_detection_t det = {0};
            det.type = sig->type;
            det.confidence = sig->confidence;
            det.method = UFT_PCDET_SIGNATURE;
            det.details = (char *)sig->description;
            det.offset = offset;
            det.track = -1;
            det.head = -1;
            det.sector = -1;
            
            result_add(result, &det);
            result->signatures_matched++;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * SafeDisc Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_pc_check_safedisc(const uint8_t *data, size_t size, char **version) {
    if (!data || size == 0) return 0;
    
    int confidence = 0;
    int detected_version = 0;
    
    /* Look for signature files */
    if (memmem_search(data, size, SAFEDISC_STXT, 4, NULL)) {
        confidence += 30;
    }
    
    if (memmem_search(data, size, SAFEDISC_CLCD, 4, NULL)) {
        confidence += 25;
    }
    
    if (memmem_search(data, size, SAFEDISC_DPLAYERX, 8, NULL)) {
        confidence += 35;
        detected_version = 2;
    }
    
    if (memmem_search(data, size, SAFEDISC_CLOKSPL, 7, NULL)) {
        confidence += 30;
    }
    
    /* Version-specific signatures */
    if (memmem_search(data, size, SAFEDISC_V1_SIG, 8, NULL)) {
        confidence += 40;
        detected_version = 1;
    }
    
    if (memmem_search(data, size, SAFEDISC_V2_SIG, 8, NULL)) {
        confidence += 40;
        detected_version = 2;
    }
    
    /* Look for 00000001.TMP pattern */
    const uint8_t tmp_pattern[] = {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x31};
    if (memmem_search(data, size, tmp_pattern, 8, NULL)) {
        confidence += 20;
    }
    
    /* Cap at 100 */
    if (confidence > 100) confidence = 100;
    
    /* Version string */
    if (version && confidence > 0) {
        if (detected_version >= 2) {
            *version = strdup("2.x");
        } else if (detected_version == 1) {
            *version = strdup("1.x");
        } else {
            *version = strdup("Unknown");
        }
    }
    
    return confidence;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * SecuROM Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_pc_check_securom(const uint8_t *data, size_t size, char **version) {
    if (!data || size == 0) return 0;
    
    int confidence = 0;
    int detected_version = 0;
    
    /* Check for SecuROM driver */
    if (memmem_search(data, size, SECUROM_SECCDRV, 7, NULL)) {
        confidence += 40;
        detected_version = 4;
    }
    
    /* CMS signatures */
    if (memmem_search(data, size, SECUROM_CMS16, 5, NULL)) {
        confidence += 35;
        detected_version = 1;
    }
    
    if (memmem_search(data, size, SECUROM_CMS32, 6, NULL)) {
        confidence += 35;
        detected_version = 4;
    }
    
    /* Product Activation */
    if (memmem_search(data, size, SECUROM_PA_SIG, 10, NULL)) {
        confidence += 45;
        detected_version = 7;
    }
    
    /* Look for .cms section in PE */
    const uint8_t cms_section[] = {0x2E, 0x63, 0x6D, 0x73};  /* ".cms" */
    if (memmem_search(data, size, cms_section, 4, NULL)) {
        confidence += 30;
    }
    
    /* Look for SecuROM anti-debug */
    const uint8_t anti_debug[] = {0x64, 0xA1, 0x30, 0x00, 0x00, 0x00};
    if (memmem_search(data, size, anti_debug, 6, NULL)) {
        confidence += 15;
    }
    
    if (confidence > 100) confidence = 100;
    
    if (version && confidence > 0) {
        char ver[16];
        snprintf(ver, sizeof(ver), "%d.x", detected_version ? detected_version : 4);
        *version = strdup(ver);
    }
    
    return confidence;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * StarForce Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_pc_check_starforce(const uint8_t *data, size_t size, char **version) {
    if (!data || size == 0) return 0;
    
    int confidence = 0;
    int detected_version = 0;
    
    /* StarForce driver */
    if (memmem_search(data, size, STARFORCE_PROTECT, 7, NULL)) {
        confidence += 35;
        detected_version = 3;
    }
    
    if (memmem_search(data, size, STARFORCE_SFDRV, 5, NULL)) {
        confidence += 40;
        detected_version = 3;
    }
    
    /* StarForce header */
    if (memmem_search(data, size, STARFORCE_HEADER, 4, NULL)) {
        confidence += 25;
    }
    
    /* Look for protection.dll */
    const uint8_t prot_dll[] = {0x70, 0x72, 0x6F, 0x74, 0x65, 0x63, 0x74, 0x69, 
                                 0x6F, 0x6E, 0x2E, 0x64, 0x6C, 0x6C};
    if (memmem_search(data, size, prot_dll, 14, NULL)) {
        confidence += 45;
    }
    
    /* SF kernel mode driver pattern */
    const uint8_t sf_kernel[] = {0x53, 0x46, 0x50, 0x72, 0x6F, 0x74};  /* "SFProt" */
    if (memmem_search(data, size, sf_kernel, 6, NULL)) {
        confidence += 30;
        detected_version = 3;
    }
    
    if (confidence > 100) confidence = 100;
    
    if (version && confidence > 0) {
        char ver[16];
        snprintf(ver, sizeof(ver), "%d.x", detected_version ? detected_version : 2);
        *version = strdup(ver);
    }
    
    return confidence;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * DOS/Floppy Protection Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_pc_protection_t uft_pc_check_dos_protection(const uint8_t *data, size_t size,
                                                 const void *geometry) {
    if (!data || size == 0) return UFT_PCPROT_UNKNOWN;
    
    (void)geometry;  /* Would use for advanced detection */
    
    /* Check for ProLock */
    if (memmem_search(data, size, PROLOCK_SIG, 7, NULL)) {
        return UFT_PCPROT_PROLOCK;
    }
    
    /* Check for SoftGuard */
    if (memmem_search(data, size, SOFTGUARD_SIG, 6, NULL)) {
        return UFT_PCPROT_SOFTGUARD;
    }
    
    /* Check for CopyLock PC */
    if (memmem_search(data, size, COPYLOCK_PC_SIG, 8, NULL)) {
        return UFT_PCPROT_COPYLOCK_PC;
    }
    
    /* Check for key disk signature - typically in boot sector */
    if (size >= 512) {
        /* Key disk often has specific patterns at sector end */
        const uint8_t key_pattern[] = {0x4B, 0x45, 0x59};  /* "KEY" */
        if (memmem_search(data, 512, key_pattern, 3, NULL)) {
            return UFT_PCPROT_KEY_DISK;
        }
    }
    
    return UFT_PCPROT_UNKNOWN;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Weak Bit Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_pc_check_weak_bits(const uint8_t *flux, size_t size,
                            int track, int head) {
    if (!flux || size == 0) return 0;
    
    (void)head;
    
    /* Analyze flux timing variance */
    /* Weak bits show high variance across reads */
    
    /* Known weak bit protection tracks */
    if (track == 6 || track == 38 || track == 39 || track == 79) {
        /* These are common protection tracks */
        
        /* Look for timing anomalies */
        int anomalies = 0;
        for (size_t i = 1; i < size; i++) {
            int diff = abs((int)flux[i] - (int)flux[i-1]);
            if (diff > 100) anomalies++;
        }
        
        if (anomalies > (int)(size / 20)) {
            return 80;  /* High confidence */
        } else if (anomalies > (int)(size / 50)) {
            return 50;  /* Medium confidence */
        }
    }
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Timing-Based Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_pc_protection_t uft_pc_check_timing(const uint8_t *flux, size_t size,
                                         uint32_t sample_rate) {
    if (!flux || size == 0) return UFT_PCPROT_UNKNOWN;
    
    (void)sample_rate;
    
    /* Analyze timing patterns for protection signatures */
    
    /* Check for intentionally long gaps */
    int long_gaps = 0;
    int current_gap = 0;
    
    for (size_t i = 0; i < size; i++) {
        if (flux[i] == 0) {
            current_gap++;
        } else {
            if (current_gap > 50) {
                long_gaps++;
            }
            current_gap = 0;
        }
    }
    
    if (long_gaps > 10) {
        return UFT_PCPROT_SECTOR_GAP;
    }
    
    /* Check for density variations */
    /* Would analyze bit rate changes */
    
    return UFT_PCPROT_UNKNOWN;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Main Scanner Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_pc_scan_result_t *uft_pc_scan(const uint8_t *data, size_t size,
                                   const uft_pc_scan_options_t *options) {
    if (!data || size == 0) return NULL;
    
    uint64_t start_time = get_time_ms();
    
    uft_pc_scan_options_t opts;
    if (options) {
        opts = *options;
    } else {
        uft_pc_scan_options_t defaults = UFT_PC_SCAN_OPTIONS_DEFAULT;
        opts = defaults;
    }
    
    uft_pc_scan_result_t *result = result_create();
    if (!result) return NULL;
    
    /* Progress callback */
    if (opts.progress) {
        opts.progress(0, "Starting scan...", opts.progress_user);
    }
    
    /* Signature scanning */
    if (opts.use_signatures) {
        if (opts.progress) {
            opts.progress(20, "Scanning signatures...", opts.progress_user);
        }
        scan_signatures(data, size, result, &opts);
    }
    
    /* SafeDisc specific */
    if (opts.check_safedisc) {
        if (opts.progress) {
            opts.progress(35, "Checking SafeDisc...", opts.progress_user);
        }
        char *ver = NULL;
        int conf = uft_pc_check_safedisc(data, size, &ver);
        if (conf >= opts.min_confidence) {
            uft_pc_detection_t det = {0};
            det.type = UFT_PCPROT_SAFEDISC_1;
            det.confidence = conf;
            det.method = UFT_PCDET_SIGNATURE;
            det.version = ver;
            det.details = strdup("SafeDisc protection detected");
            det.track = -1;
            det.head = -1;
            det.sector = -1;
            result_add(result, &det);
            free(det.details);
        }
        free(ver);
    }
    
    /* SecuROM specific */
    if (opts.check_securom) {
        if (opts.progress) {
            opts.progress(50, "Checking SecuROM...", opts.progress_user);
        }
        char *ver = NULL;
        int conf = uft_pc_check_securom(data, size, &ver);
        if (conf >= opts.min_confidence) {
            uft_pc_detection_t det = {0};
            det.type = UFT_PCPROT_SECUROM_4;
            det.confidence = conf;
            det.method = UFT_PCDET_SIGNATURE;
            det.version = ver;
            det.details = strdup("SecuROM protection detected");
            det.track = -1;
            det.head = -1;
            det.sector = -1;
            result_add(result, &det);
            free(det.details);
        }
        free(ver);
    }
    
    /* StarForce specific */
    if (opts.check_starforce) {
        if (opts.progress) {
            opts.progress(65, "Checking StarForce...", opts.progress_user);
        }
        char *ver = NULL;
        int conf = uft_pc_check_starforce(data, size, &ver);
        if (conf >= opts.min_confidence) {
            uft_pc_detection_t det = {0};
            det.type = UFT_PCPROT_STARFORCE_3;
            det.confidence = conf;
            det.method = UFT_PCDET_SIGNATURE;
            det.version = ver;
            det.details = strdup("StarForce protection detected");
            det.track = -1;
            det.head = -1;
            det.sector = -1;
            result_add(result, &det);
            free(det.details);
        }
        free(ver);
    }
    
    /* DOS/Floppy protections */
    if (opts.check_dos_protections) {
        if (opts.progress) {
            opts.progress(80, "Checking DOS protections...", opts.progress_user);
        }
        uft_pc_protection_t dos_prot = uft_pc_check_dos_protection(data, size, NULL);
        if (dos_prot != UFT_PCPROT_UNKNOWN) {
            uft_pc_detection_t det = {0};
            det.type = dos_prot;
            det.confidence = 85;
            det.method = UFT_PCDET_SIGNATURE;
            det.details = strdup("DOS-era protection detected");
            det.track = -1;
            det.head = -1;
            det.sector = -1;
            result_add(result, &det);
            free(det.details);
        }
    }
    
    /* Finalize */
    result->scan_time_ms = get_time_ms() - start_time;
    
    if (opts.progress) {
        opts.progress(100, "Scan complete", opts.progress_user);
    }
    
    return result;
}

uft_pc_scan_result_t *uft_pc_scan_file(const char *path,
                                        const uft_pc_scan_options_t *options) {
    if (!path) return NULL;
    
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    size_t size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(f);
        return NULL;
    }
    
    if (fread(data, 1, size, f) != size) { /* I/O error */ }
    fclose(f);
    
    uft_pc_scan_result_t *result = uft_pc_scan(data, size, options);
    result->files_scanned = 1;
    
    free(data);
    return result;
}

uft_pc_scan_result_t *uft_pc_scan_track(const uint8_t *track_data, size_t size,
                                         int track_num, int head,
                                         const uft_pc_scan_options_t *options) {
    uft_pc_scan_result_t *result = uft_pc_scan(track_data, size, options);
    if (result) {
        result->tracks_scanned = 1;
        
        /* Update all detections with track info */
        for (int i = 0; i < result->count; i++) {
            result->detections[i].track = track_num;
            result->detections[i].head = head;
        }
    }
    return result;
}

uft_pc_scan_result_t *uft_pc_scan_flux(const uint8_t *flux, size_t size,
                                        uint32_t sample_rate,
                                        const uft_pc_scan_options_t *options) {
    (void)options;
    
    uft_pc_scan_result_t *result = result_create();
    if (!result) return NULL;
    
    uint64_t start_time = get_time_ms();
    
    /* Timing-based detection */
    uft_pc_protection_t timing_prot = uft_pc_check_timing(flux, size, sample_rate);
    if (timing_prot != UFT_PCPROT_UNKNOWN) {
        uft_pc_detection_t det = {0};
        det.type = timing_prot;
        det.confidence = 70;
        det.method = UFT_PCDET_TIMING;
        det.details = strdup("Timing-based protection detected");
        det.track = -1;
        det.head = -1;
        det.sector = -1;
        result_add(result, &det);
        free(det.details);
    }
    
    result->scan_time_ms = get_time_ms() - start_time;
    return result;
}

void uft_pc_scan_free(uft_pc_scan_result_t *result) {
    if (!result) return;
    
    for (int i = 0; i < result->count; i++) {
        free(result->detections[i].version);
        free(result->detections[i].details);
        free(result->detections[i].filename);
    }
    free(result->detections);
    free(result);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Signature Database API
 * ═══════════════════════════════════════════════════════════════════════════════ */

const uft_pc_signature_t *uft_pc_get_signatures(int *count) {
    /* Convert internal format to public API */
    static uft_pc_signature_t sigs[32];
    static int cached_count = 0;
    
    if (cached_count == 0) {
        for (int i = 0; signature_db[i].name && cached_count < 31; i++) {
            sigs[cached_count].name = signature_db[i].name;
            sigs[cached_count].protection = signature_db[i].type;
            sigs[cached_count].pattern = signature_db[i].pattern;
            sigs[cached_count].mask = NULL;
            sigs[cached_count].length = signature_db[i].length;
            sigs[cached_count].offset = -1;
            sigs[cached_count].file_pattern = NULL;
            sigs[cached_count].confidence = signature_db[i].confidence;
            sigs[cached_count].description = signature_db[i].description;
            cached_count++;
        }
    }
    
    *count = cached_count;
    return sigs;
}

bool uft_pc_match_signature(const uft_pc_signature_t *sig,
                             const uint8_t *data, size_t size,
                             size_t *offset) {
    if (!sig || !data) return false;
    return memmem_search(data, size, sig->pattern, sig->length, offset);
}

int uft_pc_find_signatures(const uint8_t *data, size_t size,
                            const uft_pc_signature_t **matches,
                            int max_matches) {
    int count = 0;
    int sig_count;
    const uft_pc_signature_t *sigs = uft_pc_get_signatures(&sig_count);
    
    for (int i = 0; i < sig_count && count < max_matches; i++) {
        size_t offset;
        if (uft_pc_match_signature(&sigs[i], data, size, &offset)) {
            matches[count++] = &sigs[i];
        }
    }
    
    return count;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Result Analysis
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_pc_protection_t uft_pc_get_primary(const uft_pc_scan_result_t *result) {
    return result ? result->primary : UFT_PCPROT_UNKNOWN;
}

int uft_pc_has_protection(const uft_pc_scan_result_t *result,
                           uft_pc_protection_t protection) {
    if (!result) return 0;
    
    int max_conf = 0;
    for (int i = 0; i < result->count; i++) {
        if (result->detections[i].type == protection) {
            if (result->detections[i].confidence > max_conf) {
                max_conf = result->detections[i].confidence;
            }
        }
    }
    return max_conf;
}

int uft_pc_get_detections(const uft_pc_scan_result_t *result,
                           uft_pc_protection_t protection,
                           uft_pc_detection_t *out, int max_out) {
    if (!result || !out) return 0;
    
    int count = 0;
    for (int i = 0; i < result->count && count < max_out; i++) {
        if (result->detections[i].type == protection) {
            out[count++] = result->detections[i];
        }
    }
    return count;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Output Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_pc_print_result(const uft_pc_scan_result_t *result) {
    if (!result) {
        printf("No scan result\n");
        return;
    }
    
    printf("PC Protection Scan Result\n");
    printf("=========================\n");
    printf("Protected: %s\n", result->is_protected ? "YES" : "NO");
    printf("Detections: %d\n", result->count);
    printf("Scan time: %lu ms\n", (unsigned long)result->scan_time_ms);
    
    if (result->primary != UFT_PCPROT_UNKNOWN) {
        printf("Primary: %s (%d%%)\n",
               uft_pc_protection_name(result->primary),
               result->primary_confidence);
    }
}

void uft_pc_print_detail(const uft_pc_scan_result_t *result) {
    uft_pc_print_result(result);
    
    if (!result || result->count == 0) return;
    
    printf("\nDetections:\n");
    for (int i = 0; i < result->count; i++) {
        uft_pc_detection_t *d = &result->detections[i];
        printf("  %d. %s\n", i + 1, uft_pc_protection_name(d->type));
        printf("     Confidence: %d%%\n", d->confidence);
        printf("     Method: %s\n", uft_pc_method_name(d->method));
        if (d->version) printf("     Version: %s\n", d->version);
        if (d->details) printf("     Details: %s\n", d->details);
        if (d->track >= 0) printf("     Track: %d/%d\n", d->track, d->head);
    }
}

char *uft_pc_result_to_json(const uft_pc_scan_result_t *result, bool pretty) {
    size_t size = 4096 + (result ? result->count * 512 : 0);
    char *json = malloc(size);
    if (!json) return NULL;
    
    const char *nl = pretty ? "\n" : "";
    const char *sp = pretty ? "  " : "";
    
    int pos = 0;
    pos += snprintf(json + pos, size - pos, "{%s", nl);
    
    if (result) {
        pos += snprintf(json + pos, size - pos,
            "%s\"protected\": %s,%s"
            "%s\"detection_count\": %d,%s"
            "%s\"scan_time_ms\": %lu,%s",
            sp, result->is_protected ? "true" : "false", nl,
            sp, result->count, nl,
            sp, (unsigned long)result->scan_time_ms, nl);
        
        if (result->primary != UFT_PCPROT_UNKNOWN) {
            pos += snprintf(json + pos, size - pos,
                "%s\"primary\": \"%s\",%s"
                "%s\"primary_confidence\": %d,%s",
                sp, uft_pc_protection_name(result->primary), nl,
                sp, result->primary_confidence, nl);
        }
        
        pos += snprintf(json + pos, size - pos, "%s\"detections\": [%s", sp, nl);
        for (int i = 0; i < result->count && pos < (int)size - 256; i++) {
            uft_pc_detection_t *d = &result->detections[i];
            pos += snprintf(json + pos, size - pos,
                "%s%s{\"type\": \"%s\", \"confidence\": %d, \"method\": \"%s\"}%s%s",
                sp, sp,
                uft_pc_protection_name(d->type),
                d->confidence,
                uft_pc_method_name(d->method),
                (i < result->count - 1) ? "," : "",
                nl);
        }
        pos += snprintf(json + pos, size - pos, "%s]%s", sp, nl);
    }
    
    pos += snprintf(json + pos, size - pos, "}%s", nl);
    
    return json;
}

char *uft_pc_result_to_report(const uft_pc_scan_result_t *result) {
    return uft_pc_result_to_json(result, true);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

const char *uft_pc_protection_name(uft_pc_protection_t protection) {
    switch (protection) {
        case UFT_PCPROT_UNKNOWN:       return "Unknown";
        case UFT_PCPROT_SAFEDISC_1:    return "SafeDisc 1.x";
        case UFT_PCPROT_SAFEDISC_2:    return "SafeDisc 2.x";
        case UFT_PCPROT_SAFEDISC_3:    return "SafeDisc 3.x";
        case UFT_PCPROT_SAFEDISC_4:    return "SafeDisc 4.x";
        case UFT_PCPROT_SECUROM_1:     return "SecuROM 1.x";
        case UFT_PCPROT_SECUROM_4:     return "SecuROM 4.x";
        case UFT_PCPROT_SECUROM_5:     return "SecuROM 5.x";
        case UFT_PCPROT_SECUROM_7:     return "SecuROM 7.x";
        case UFT_PCPROT_SECUROM_PA:    return "SecuROM PA";
        case UFT_PCPROT_STARFORCE_1:   return "StarForce 1.x";
        case UFT_PCPROT_STARFORCE_2:   return "StarForce 2.x";
        case UFT_PCPROT_STARFORCE_3:   return "StarForce 3.x";
        case UFT_PCPROT_PROLOCK:       return "ProLock";
        case UFT_PCPROT_SOFTGUARD:     return "SoftGuard";
        case UFT_PCPROT_COPYLOCK_PC:   return "CopyLock PC";
        case UFT_PCPROT_KEY_DISK:      return "Key Disk";
        case UFT_PCPROT_WEAK_SECTOR:   return "Weak Sector";
        case UFT_PCPROT_LONG_TRACK:    return "Long Track";
        case UFT_PCPROT_SHORT_TRACK:   return "Short Track";
        case UFT_PCPROT_EXTRA_TRACK:   return "Extra Track";
        case UFT_PCPROT_CRC_ERROR:     return "CRC Error";
        case UFT_PCPROT_SECTOR_GAP:    return "Sector Gap";
        default:                        return "Unknown";
    }
}

const char *uft_pc_protection_desc(uft_pc_protection_t protection) {
    switch (protection) {
        case UFT_PCPROT_SAFEDISC_1:
            return "Macrovision SafeDisc 1.x copy protection (1998-2000)";
        case UFT_PCPROT_SAFEDISC_2:
            return "Macrovision SafeDisc 2.x copy protection (2000-2003)";
        case UFT_PCPROT_SECUROM_4:
            return "Sony DADC SecuROM 4.x copy protection";
        case UFT_PCPROT_SECUROM_PA:
            return "SecuROM with Product Activation";
        case UFT_PCPROT_STARFORCE_3:
            return "StarForce 3.x/Pro copy protection";
        case UFT_PCPROT_PROLOCK:
            return "Vault Corporation ProLock floppy protection";
        case UFT_PCPROT_WEAK_SECTOR:
            return "Floppy protection using weak/random bits";
        default:
            return "Copy protection scheme";
    }
}

const char *uft_pc_protection_category(uft_pc_protection_t protection) {
    if (protection >= UFT_PCPROT_SAFEDISC_1 && protection <= UFT_PCPROT_SAFEDISC_4)
        return "SafeDisc";
    if (protection >= UFT_PCPROT_SECUROM_1 && protection <= UFT_PCPROT_SECUROM_PA)
        return "SecuROM";
    if (protection >= UFT_PCPROT_STARFORCE_1 && protection <= UFT_PCPROT_STARFORCE_3)
        return "StarForce";
    if (protection >= UFT_PCPROT_PROLOCK && protection <= UFT_PCPROT_COPYLOCK_PC)
        return "DOS Era";
    if (protection >= UFT_PCPROT_WEAK_SECTOR && protection <= UFT_PCPROT_SECTOR_GAP)
        return "Floppy Protection";
    return "Unknown";
}

const char *uft_pc_method_name(uft_pc_detection_method_t method) {
    switch (method) {
        case UFT_PCDET_SIGNATURE:  return "Signature";
        case UFT_PCDET_STRUCTURE:  return "Structure";
        case UFT_PCDET_TIMING:     return "Timing";
        case UFT_PCDET_WEAK_BITS:  return "Weak Bits";
        case UFT_PCDET_GEOMETRY:   return "Geometry";
        case UFT_PCDET_CHECKSUM:   return "Checksum";
        case UFT_PCDET_BEHAVIORAL: return "Behavioral";
        case UFT_PCDET_HEURISTIC:  return "Heuristic";
        default:                   return "Unknown";
    }
}

bool uft_pc_is_cd_protection(uft_pc_protection_t protection) {
    return (protection >= UFT_PCPROT_SAFEDISC_1 && protection <= UFT_PCPROT_STARFORCE_3);
}

bool uft_pc_is_floppy_protection(uft_pc_protection_t protection) {
    return (protection >= UFT_PCPROT_PROLOCK && protection <= UFT_PCPROT_SECTOR_GAP);
}

bool uft_pc_uses_weak_bits(uft_pc_protection_t protection) {
    return (protection == UFT_PCPROT_WEAK_SECTOR ||
            protection == UFT_PCPROT_SAFEDISC_1 ||
            protection == UFT_PCPROT_SAFEDISC_2);
}

bool uft_pc_is_timing_dependent(uft_pc_protection_t protection) {
    return (protection == UFT_PCPROT_SECTOR_GAP ||
            protection == UFT_PCPROT_STARFORCE_3);
}

uft_format_t uft_pc_recommended_format(uft_pc_protection_t protection) {
    /* Recommend flux formats for protected disks */
    if (uft_pc_uses_weak_bits(protection)) {
        return UFT_FORMAT_SCP;  /* SCP preserves weak bits */
    }
    if (uft_pc_is_timing_dependent(protection)) {
        return UFT_FORMAT_SCP;  /* SCP preserves timing */
    }
    if (uft_pc_is_floppy_protection(protection)) {
        return UFT_FORMAT_SCP;  /* General flux preservation */
    }
    return UFT_FORMAT_IMG;  /* Standard sector image */
}
