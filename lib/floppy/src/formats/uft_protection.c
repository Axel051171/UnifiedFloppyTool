#define _POSIX_C_SOURCE 200809L
/**
 * @file uft_protection.c
 * @brief Copy protection detection implementation
 */

#include "formats/uft_protection.h"
/* Platform compatibility for ssize_t */
#ifdef _MSC_VER
    #include <BaseTsd.h>
    typedef SSIZE_T ssize_t;
#else
    #include <sys/types.h>
#endif
#include "encoding/uft_mfm.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Protection Scheme Names
 * ============================================================================ */

static const char* protection_names[] = {
    [UFT_PROT_NONE]             = "None",
    [UFT_PROT_WEAK_BITS]        = "Weak Bits",
    [UFT_PROT_FLUX_REVERSAL]    = "Missing Flux Reversals",
    [UFT_PROT_EXTRA_SECTORS]    = "Extra Sectors",
    [UFT_PROT_MISSING_SECTORS]  = "Missing Sectors",
    [UFT_PROT_DUPLICATE_SECTORS]= "Duplicate Sector IDs",
    [UFT_PROT_BAD_SECTORS]      = "Intentional Bad Sectors",
    [UFT_PROT_DELETED_DATA]     = "Deleted Data Marks",
    [UFT_PROT_NONSTANDARD_SIZE] = "Non-Standard Sector Size",
    [UFT_PROT_LONG_TRACK]       = "Long Track",
    [UFT_PROT_SHORT_TRACK]      = "Short Track",
    [UFT_PROT_HALF_TRACK]       = "Half Track Data",
    [UFT_PROT_EXTRA_TRACK]      = "Extra Tracks",
    [UFT_PROT_VARIABLE_DENSITY] = "Variable Density",
    [UFT_PROT_SPEED_VARIATION]  = "Speed Variation",
    [UFT_PROT_TIMING_BASED]     = "Timing-Based Protection",
    [UFT_PROT_NONSTANDARD_GAP]  = "Non-Standard Gap",
    [UFT_PROT_UNUSUAL_SYNC]     = "Unusual Sync Pattern",
    [UFT_PROT_MIXED_FORMAT]     = "Mixed MFM/FM Format",
    [UFT_PROT_PROLOK]           = "Vault ProLok",
    [UFT_PROT_SOFTGUARD]        = "SoftGuard SuperLok",
    [UFT_PROT_SPIRADISC]        = "Spiradisk",
    [UFT_PROT_COPYLOCK]         = "CopyLock (Amiga)",
    [UFT_PROT_EVERLOCK]         = "Everlock",
    [UFT_PROT_FBCOPY]           = "Fat Bits (C64)",
    [UFT_PROT_V_MAX]            = "V-Max (C64)",
    [UFT_PROT_RAPIDLOK]         = "RapidLok (C64)",
};

const char* uft_protection_type_name(uft_protection_type_t type) {
    if (type >= UFT_PROT_COUNT) return "Unknown";
    return protection_names[type];
}

/* ============================================================================
 * Known Protection Signatures
 * ============================================================================ */

/* ProLok signature on track 39 */
static const uint8_t prolok_sig[] = { 0x50, 0x52, 0x4F, 0x4C, 0x4F, 0x4B };

/* CopyLock Amiga signature */
static const uint8_t copylock_sig[] = { 0x00, 0x00, 0xF3, 0xF9, 0x00, 0x00 };

/* V-Max C64 signature */
static const uint8_t vmax_sig[] = { 0x56, 0x2D, 0x4D, 0x41, 0x58 }; /* "V-MAX" */

/* RapidLok C64 - unusual sync pattern */
static const uint8_t rapidlok_sync[] = { 0xFF, 0xFF, 0xFF, 0x55, 0x55 };

const uft_protection_signature_t uft_protection_signatures[] = {
    { UFT_PROT_PROLOK, "ProLok", prolok_sig, sizeof(prolok_sig), 39, 0xFF, 0 },
    { UFT_PROT_COPYLOCK, "CopyLock", copylock_sig, sizeof(copylock_sig), 0xFF, 0xFF, 0 },
    { UFT_PROT_V_MAX, "V-Max", vmax_sig, sizeof(vmax_sig), 0xFF, 0xFF, 0 },
    { UFT_PROT_RAPIDLOK, "RapidLok", rapidlok_sync, sizeof(rapidlok_sync), 0xFF, 0xFF, 0 },
};

const size_t uft_protection_signature_count = 
    sizeof(uft_protection_signatures) / sizeof(uft_protection_signatures[0]);

/* ============================================================================
 * Report Management
 * ============================================================================ */

#define INITIAL_HIT_CAPACITY 32

uft_protection_report_t* uft_protection_report_create(void) {
    uft_protection_report_t *report = calloc(1, sizeof(uft_protection_report_t));
    if (!report) return NULL;
    
    report->hits = calloc(INITIAL_HIT_CAPACITY, sizeof(uft_protection_hit_t));
    if (!report->hits) {
        free(report);
        return NULL;
    }
    
    report->hit_capacity = INITIAL_HIT_CAPACITY;
    return report;
}

void uft_protection_report_free(uft_protection_report_t *report) {
    if (report) {
        free(report->hits);
        free(report);
    }
}

bool uft_protection_report_add(uft_protection_report_t *report,
                                const uft_protection_hit_t *hit) {
    if (!report || !hit) return false;
    
    /* Expand if needed */
    if (report->hit_count >= report->hit_capacity) {
        size_t new_cap = report->hit_capacity * 2;
        uft_protection_hit_t *new_hits = realloc(report->hits,
            new_cap * sizeof(uft_protection_hit_t));
        if (!new_hits) return false;
        report->hits = new_hits;
        report->hit_capacity = new_cap;
    }
    
    report->hits[report->hit_count++] = *hit;
    
    /* Update summary flags */
    switch (hit->type) {
        case UFT_PROT_WEAK_BITS:
        case UFT_PROT_FLUX_REVERSAL:
            report->has_weak_bits = true;
            break;
            
        case UFT_PROT_SPEED_VARIATION:
        case UFT_PROT_TIMING_BASED:
        case UFT_PROT_VARIABLE_DENSITY:
            report->has_timing_protection = true;
            break;
            
        case UFT_PROT_EXTRA_SECTORS:
        case UFT_PROT_MISSING_SECTORS:
        case UFT_PROT_DUPLICATE_SECTORS:
        case UFT_PROT_BAD_SECTORS:
        case UFT_PROT_DELETED_DATA:
        case UFT_PROT_NONSTANDARD_SIZE:
            report->has_sector_anomalies = true;
            break;
            
        case UFT_PROT_LONG_TRACK:
        case UFT_PROT_SHORT_TRACK:
        case UFT_PROT_HALF_TRACK:
        case UFT_PROT_EXTRA_TRACK:
            report->has_track_anomalies = true;
            break;
            
        default:
            break;
    }
    
    /* Update primary scheme if this hit has higher confidence */
    if (hit->confidence > report->overall_confidence) {
        report->overall_confidence = hit->confidence;
        report->primary_scheme = hit->type;
    }
    
    return true;
}

/* ============================================================================
 * Weak Bit Detection
 * ============================================================================ */

size_t uft_find_weak_bits(
    const uint8_t **reads,
    size_t read_count,
    size_t track_len,
    uft_weak_region_t *regions,
    size_t max_regions
) {
    if (!reads || read_count < 2 || !regions || max_regions == 0) {
        return 0;
    }
    
    size_t region_count = 0;
    bool in_weak_region = false;
    uft_weak_region_t current = {0};
    
    for (size_t i = 0; i < track_len && region_count < max_regions; i++) {
        /* Compare all reads at this position */
        uint8_t min_val = reads[0][i];
        uint8_t max_val = reads[0][i];
        bool differs = false;
        
        for (size_t r = 1; r < read_count; r++) {
            uint8_t val = reads[r][i];
            if (val != reads[0][i]) {
                differs = true;
            }
            if (val < min_val) min_val = val;
            if (val > max_val) max_val = val;
        }
        
        if (differs) {
            if (!in_weak_region) {
                /* Start new region */
                in_weak_region = true;
                current.offset = (uint32_t)i;
                current.length = 8;  /* bits */
                current.min_value = min_val;
                current.max_value = max_val;
                current.variation_count = 1;
            } else {
                /* Extend current region */
                current.length += 8;
                if (min_val < current.min_value) current.min_value = min_val;
                if (max_val > current.max_value) current.max_value = max_val;
            }
        } else if (in_weak_region) {
            /* End current region */
            regions[region_count++] = current;
            in_weak_region = false;
        }
    }
    
    /* Don't forget the last region */
    if (in_weak_region && region_count < max_regions) {
        regions[region_count++] = current;
    }
    
    return region_count;
}

size_t uft_find_flux_anomalies(
    const uint32_t *flux_data,
    size_t flux_len,
    uint32_t threshold,
    uft_weak_region_t *regions,
    size_t max_regions
) {
    if (!flux_data || flux_len == 0 || !regions) {
        return 0;
    }
    
    size_t region_count = 0;
    
    /* Look for unusually long or short flux intervals */
    for (size_t i = 1; i < flux_len && region_count < max_regions; i++) {
        uint32_t interval = flux_data[i];
        
        /* Anomaly: interval much longer than expected (missing flux) */
        if (interval > threshold * 3) {
            regions[region_count].offset = (uint32_t)i;
            regions[region_count].length = 1;
            regions[region_count].min_value = 0;
            regions[region_count].max_value = 0;
            regions[region_count].variation_count = 0;
            region_count++;
        }
    }
    
    return region_count;
}

/* ============================================================================
 * Sector Analysis
 * ============================================================================ */

size_t uft_analyze_track_sectors(
    const uint8_t *track_data,
    size_t track_len,
    uft_sector_info_t *sectors,
    size_t max_sectors
) {
    if (!track_data || !sectors || max_sectors == 0) {
        return 0;
    }
    
    size_t sector_count = 0;
    size_t pos = 0;
    
    while (pos < track_len - 20 && sector_count < max_sectors) {
        /* Look for sync pattern (3x A1 with missing clock = 0x4489) */
        ssize_t sync_pos = uft_mfm_find_sync(track_data, track_len, pos);
        if (sync_pos < 0) break;
        
        pos = (size_t)sync_pos + 6;  /* Skip sync bytes */
        if (pos >= track_len - 10) break;
        
        /* Decode address mark */
        uint16_t mfm_word = (uint16_t)((track_data[pos] << 8) | track_data[pos + 1]);
        uint8_t am = uft_mfm_decode_byte(mfm_word);
        
        if (am == UFT_MFM_AM_ID) {
            /* Found sector ID */
            uft_sector_info_t *sec = &sectors[sector_count];
            memset(sec, 0, sizeof(*sec));
            sec->track_offset = (uint32_t)sync_pos;
            
            pos += 2;
            
            /* Decode ID field */
            mfm_word = (uint16_t)((track_data[pos] << 8) | track_data[pos+1]);
            sec->cylinder = uft_mfm_decode_byte(mfm_word);
            pos += 2;
            
            mfm_word = (uint16_t)((track_data[pos] << 8) | track_data[pos+1]);
            sec->head = uft_mfm_decode_byte(mfm_word);
            pos += 2;
            
            mfm_word = (uint16_t)((track_data[pos] << 8) | track_data[pos+1]);
            sec->sector = uft_mfm_decode_byte(mfm_word);
            pos += 2;
            
            mfm_word = (uint16_t)((track_data[pos] << 8) | track_data[pos+1]);
            sec->size_code = uft_mfm_decode_byte(mfm_word);
            pos += 2;
            
            /* Calculate actual size */
            sec->actual_size = (uint16_t)(128 << sec->size_code);
            
            /* Read CRC */
            mfm_word = (uint16_t)((track_data[pos] << 8) | track_data[pos+1]);
            uint8_t crc_hi = uft_mfm_decode_byte(mfm_word);
            pos += 2;
            mfm_word = (uint16_t)((track_data[pos] << 8) | track_data[pos+1]);
            uint8_t crc_lo = uft_mfm_decode_byte(mfm_word);
            sec->header_crc = (uint16_t)((crc_hi << 8) | crc_lo);
            
            /* Calculate expected CRC */
            uint8_t id_data[] = { 0xA1, 0xA1, 0xA1, UFT_MFM_AM_ID,
                                  sec->cylinder, sec->head, sec->sector, sec->size_code };
            sec->calc_header_crc = uft_mfm_crc_ccitt(id_data, sizeof(id_data), UFT_CRC_CCITT_INIT);
            
            /* Check for CRC error */
            if (sec->header_crc != sec->calc_header_crc) {
                sec->status = UFT_SECTOR_BAD_CRC;
            } else {
                sec->status = UFT_SECTOR_OK;
            }
            
            /* Check for duplicates */
            for (size_t j = 0; j < sector_count; j++) {
                if (sectors[j].cylinder == sec->cylinder &&
                    sectors[j].head == sec->head &&
                    sectors[j].sector == sec->sector) {
                    sec->status = UFT_SECTOR_DUPLICATE;
                    break;
                }
            }
            
            /* Check for non-standard size */
            if (sec->size_code > 3) {
                sec->status = UFT_SECTOR_WRONG_SIZE;
            }
            
            sector_count++;
        } else if (am == UFT_MFM_AM_DATA || am == UFT_MFM_AM_DELETED) {
            /* Data field without preceding ID - unusual */
            pos += 2;
        } else {
            pos += 2;
        }
    }
    
    return sector_count;
}

/* ============================================================================
 * Track Protection Analysis
 * ============================================================================ */

size_t uft_analyze_track_protection(
    const uint8_t *track_data,
    size_t track_len,
    uint8_t track,
    uint8_t head,
    uft_protection_report_t *report
) {
    if (!track_data || !report) return 0;
    
    size_t initial_hits = report->hit_count;
    
    /* Analyze sectors */
    uft_sector_info_t sectors[64];
    size_t sector_count = uft_analyze_track_sectors(track_data, track_len, sectors, 64);
    
    /* Check for sector anomalies */
    uint8_t seen_sectors[256] = {0};
    size_t bad_crc_count = 0;
    size_t deleted_count = 0;
    size_t duplicate_count = 0;
    
    for (size_t i = 0; i < sector_count; i++) {
        uft_sector_info_t *sec = &sectors[i];
        
        if (sec->status == UFT_SECTOR_BAD_CRC) {
            bad_crc_count++;
        }
        if (sec->status == UFT_SECTOR_DUPLICATE) {
            duplicate_count++;
        }
        
        /* Track seen sectors */
        if (seen_sectors[sec->sector]) {
            duplicate_count++;
        }
        seen_sectors[sec->sector] = 1;
    }
    
    /* Report bad CRC sectors */
    if (bad_crc_count > 0) {
        uft_protection_hit_t hit = {
            .type = UFT_PROT_BAD_SECTORS,
            .confidence = bad_crc_count > 2 ? UFT_CONF_HIGH : UFT_CONF_MEDIUM,
            .track = track,
            .head = head,
        };
        snprintf(hit.description, sizeof(hit.description),
                 "%zu sectors with CRC errors", bad_crc_count);
        uft_protection_report_add(report, &hit);
    }
    
    /* Report duplicate sectors */
    if (duplicate_count > 0) {
        uft_protection_hit_t hit = {
            .type = UFT_PROT_DUPLICATE_SECTORS,
            .confidence = UFT_CONF_HIGH,
            .track = track,
            .head = head,
        };
        snprintf(hit.description, sizeof(hit.description),
                 "%zu duplicate sector IDs", duplicate_count);
        uft_protection_report_add(report, &hit);
    }
    
    /* Check track length */
    size_t expected_len = uft_calc_track_length(250, 300);  /* DD default */
    if (uft_is_unusual_track_length(track_len, expected_len, 10)) {
        uft_protection_hit_t hit = {
            .track = track,
            .head = head,
            .length = (uint32_t)track_len,
        };
        
        if (track_len > expected_len) {
            hit.type = UFT_PROT_LONG_TRACK;
            hit.confidence = UFT_CONF_MEDIUM;
            snprintf(hit.description, sizeof(hit.description),
                     "Track is %zu bytes longer than expected",
                     track_len - expected_len);
        } else {
            hit.type = UFT_PROT_SHORT_TRACK;
            hit.confidence = UFT_CONF_LOW;
            snprintf(hit.description, sizeof(hit.description),
                     "Track is %zu bytes shorter than expected",
                     expected_len - track_len);
        }
        uft_protection_report_add(report, &hit);
    }
    
    /* Check for known protection signatures */
    for (size_t s = 0; s < uft_protection_signature_count; s++) {
        const uft_protection_signature_t *sig = &uft_protection_signatures[s];
        
        /* Check track constraint */
        if (sig->track != 0xFF && sig->track != track) continue;
        
        /* Search for signature */
        for (size_t i = 0; i <= track_len - sig->sig_len; i++) {
            if (memcmp(&track_data[i], sig->signature, sig->sig_len) == 0) {
                uft_protection_hit_t hit = {
                    .type = sig->type,
                    .confidence = UFT_CONF_HIGH,
                    .track = track,
                    .head = head,
                    .offset = (uint32_t)i,
                    .length = (uint32_t)sig->sig_len,
                };
                snprintf(hit.description, sizeof(hit.description),
                         "%s signature found at offset %zu", sig->name, i);
                uft_protection_report_add(report, &hit);
                break;  /* Only report once per signature */
            }
        }
    }
    
    return report->hit_count - initial_hits;
}

/* ============================================================================
 * Protection Scheme Detection
 * ============================================================================ */

uft_protection_type_t uft_detect_protection_scheme(
    const uint8_t *disk_data,
    size_t disk_size,
    uft_protection_report_t *report
) {
    if (!disk_data || !report) return UFT_PROT_NONE;
    
    /* This is a simplified detection - full implementation would
     * need to parse the disk image format first */
    
    /* Search entire image for known signatures */
    for (size_t s = 0; s < uft_protection_signature_count; s++) {
        const uft_protection_signature_t *sig = &uft_protection_signatures[s];
        
        for (size_t i = 0; i <= disk_size - sig->sig_len; i++) {
            if (memcmp(&disk_data[i], sig->signature, sig->sig_len) == 0) {
                uft_protection_hit_t hit = {
                    .type = sig->type,
                    .confidence = UFT_CONF_CERTAIN,
                    .offset = (uint32_t)i,
                    .length = (uint32_t)sig->sig_len,
                };
                snprintf(hit.description, sizeof(hit.description),
                         "%s detected at disk offset 0x%zX", sig->name, i);
                uft_protection_report_add(report, &hit);
            }
        }
    }
    
    return report->primary_scheme;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

size_t uft_calc_track_length(uint32_t data_rate, uint32_t rpm) {
    /* Track length = (data_rate * 60) / (rpm * 8) bytes
     * For 250 kbps @ 300 RPM: (250000 * 60) / (300 * 8) = 6250 bytes */
    return (size_t)((uint64_t)data_rate * 1000 * 60 / (rpm * 8));
}

bool uft_is_unusual_track_length(size_t track_len, size_t expected_len,
                                  uint8_t tolerance) {
    size_t margin = expected_len * tolerance / 100;
    return track_len < (expected_len - margin) || 
           track_len > (expected_len + margin);
}

size_t uft_generate_protection_report(
    const uft_protection_report_t *report,
    char *output,
    size_t output_len
) {
    if (!report) return 0;
    
    size_t written = 0;
    
    /* If output is NULL or len is 0, just calculate required size */
    bool dry_run = (output == NULL || output_len == 0);
    
    #define APPEND(...) do { \
        int n = dry_run ? snprintf(NULL, 0, __VA_ARGS__) : \
                          snprintf(output + written, output_len - written, __VA_ARGS__); \
        if (n > 0) written += (size_t)n; \
    } while(0)
    
    APPEND("=== Copy Protection Analysis Report ===\n\n");
    
    APPEND("Summary:\n");
    APPEND("  Total anomalies found: %zu\n", report->hit_count);
    APPEND("  Weak bits detected: %s\n", report->has_weak_bits ? "Yes" : "No");
    APPEND("  Timing protection: %s\n", report->has_timing_protection ? "Yes" : "No");
    APPEND("  Sector anomalies: %s\n", report->has_sector_anomalies ? "Yes" : "No");
    APPEND("  Track anomalies: %s\n", report->has_track_anomalies ? "Yes" : "No");
    
    if (report->primary_scheme != UFT_PROT_NONE) {
        APPEND("\nPrimary protection scheme: %s (confidence: %d%%)\n",
               uft_protection_type_name(report->primary_scheme),
               report->overall_confidence);
    }
    
    APPEND("\nDetailed Findings:\n");
    APPEND("-----------------\n");
    
    for (size_t i = 0; i < report->hit_count; i++) {
        const uft_protection_hit_t *hit = &report->hits[i];
        APPEND("\n[%zu] %s\n", i + 1, uft_protection_type_name(hit->type));
        APPEND("    Location: Track %u, Head %u", hit->track, hit->head);
        if (hit->sector != 0xFF) {
            APPEND(", Sector %u", hit->sector);
        }
        APPEND("\n");
        if (hit->offset > 0) {
            APPEND("    Offset: 0x%X, Length: %u bytes\n", hit->offset, hit->length);
        }
        APPEND("    Confidence: %d%%\n", hit->confidence);
        APPEND("    Description: %s\n", hit->description);
    }
    
    #undef APPEND
    
    return written;
}
