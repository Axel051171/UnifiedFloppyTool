/**
 * @file uft_protection.c
 * @brief Unified Protection Detection Implementation
 * 
 * HAFTUNGSMODUS: Konsolidierte Protection-Erkennung
 */

#include "uft/forensic/uft_protection.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

// ============================================================================
// FINGERPRINT DATABASE
// ============================================================================

typedef struct {
    uft_prot_scheme_t scheme;
    const char* name;
    const char* description;
    uft_platform_t platform;
    uft_prot_technique_t techniques;
    const uint8_t* signature;
    size_t sig_length;
    int sig_track;
    int sig_offset;
} fingerprint_entry_t;

// RapidLok v1 signature (sync + GCR pattern)
static const uint8_t SIG_RAPIDLOK1[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x52};

// V-MAX signature (halftrack marker)
static const uint8_t SIG_VMAX[] = {0x55, 0xAA, 0x55, 0xAA};

// CopyLock long track marker
static const uint8_t SIG_COPYLOCK[] = {0x44, 0x89, 0x44, 0x89};

static const fingerprint_entry_t FINGERPRINTS[] = {
    {UFT_PROT_CBM_RAPIDLOK, "RapidLok", "CBM RapidLok v1", UFT_PLATFORM_CBM,
     UFT_PROT_TECH_WEAK_BITS | UFT_PROT_TECH_SYNC_ANOMALY,
     SIG_RAPIDLOK1, 6, 18, 0},
    
    {UFT_PROT_CBM_VMAX, "V-MAX", "CBM V-MAX protection", UFT_PLATFORM_CBM,
     UFT_PROT_TECH_HALF_TRACK | UFT_PROT_TECH_FAT_TRACK,
     SIG_VMAX, 4, 20, 0},
    
    {UFT_PROT_AMI_COPYLOCK, "CopyLock", "Amiga CopyLock", UFT_PLATFORM_AMIGA,
     UFT_PROT_TECH_TRACK_LENGTH | UFT_PROT_TECH_TIMING,
     SIG_COPYLOCK, 4, 79, 0},
    
    {0, NULL, NULL, 0, 0, NULL, 0, 0, 0}
};

// ============================================================================
// CONTEXT INIT
// ============================================================================

void uft_protection_context_init(uft_protection_context_t* ctx) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    
    ctx->hint_platform = UFT_PLATFORM_UNKNOWN;
    ctx->deep_scan = false;
    ctx->detect_weak_bits = true;
    ctx->detect_sync = true;
    ctx->detect_halftrack = false;
    ctx->max_revolutions = 5;
}

// ============================================================================
// WEAK BIT ANALYSIS
// ============================================================================

int uft_protection_analyze_weak_bits(const uint8_t** revolutions,
                                     const size_t* sizes,
                                     int count,
                                     uint8_t* weak_map,
                                     size_t* weak_count) {
    if (!revolutions || !sizes || !weak_map || count < 2) return -1;
    
    size_t min_size = sizes[0];
    for (int i = 1; i < count; i++) {
        if (sizes[i] < min_size) min_size = sizes[i];
    }
    
    memset(weak_map, 0, min_size);
    size_t weak = 0;
    
    // Compare bit-by-bit across revolutions
    for (size_t byte = 0; byte < min_size; byte++) {
        uint8_t xor_result = 0;
        for (int r = 1; r < count; r++) {
            xor_result |= revolutions[0][byte] ^ revolutions[r][byte];
        }
        
        // Mark differing bits
        if (xor_result) {
            weak_map[byte] = xor_result;
            for (int b = 0; b < 8; b++) {
                if (xor_result & (1 << b)) weak++;
            }
        }
    }
    
    if (weak_count) *weak_count = weak;
    return 0;
}

// ============================================================================
// SYNC PATTERN ANALYSIS
// ============================================================================

static int count_sync_patterns(const uint8_t* data, size_t size,
                               uint64_t pattern, int pattern_bits) {
    int count = 0;
    uint64_t mask = (1ULL << pattern_bits) - 1;
    uint64_t window = 0;
    
    for (size_t i = 0; i < size; i++) {
        for (int b = 7; b >= 0; b--) {
            window = ((window << 1) | ((data[i] >> b) & 1)) & mask;
            if (window == pattern) count++;
        }
    }
    
    return count;
}

int uft_protection_analyze_sync(const uint8_t* track_data,
                                size_t track_size,
                                uft_platform_t platform,
                                uft_protection_result_t* result) {
    if (!track_data || !result) return -1;
    
    // Expected sync patterns by platform
    uint64_t expected_sync;
    int sync_bits;
    int expected_count;
    
    switch (platform) {
        case UFT_PLATFORM_CBM:
            expected_sync = 0x3FF;  // 10 consecutive 1s
            sync_bits = 10;
            expected_count = 21;    // D64 has ~21 syncs per track
            break;
        case UFT_PLATFORM_AMIGA:
            expected_sync = 0x4489; // MFM sync
            sync_bits = 16;
            expected_count = 11;
            break;
        default:
            return -1;
    }
    
    int actual = count_sync_patterns(track_data, track_size, expected_sync, sync_bits);
    
    // Check for anomalies
    if (abs(actual - expected_count) > 2) {
        if (result->sync_anomaly_count < 32) {
            result->sync_anomaly[result->sync_anomaly_count].expected_pattern = expected_sync;
            result->sync_anomaly[result->sync_anomaly_count].actual_pattern = actual;
            result->sync_anomaly_count++;
        }
        result->techniques |= UFT_PROT_TECH_SYNC_ANOMALY;
    }
    
    return 0;
}

// ============================================================================
// TRACK LENGTH ANALYSIS
// ============================================================================

int uft_protection_analyze_track_length(const uint8_t* disk_data,
                                        size_t disk_size,
                                        int track_count,
                                        uft_platform_t platform,
                                        uft_protection_result_t* result) {
    if (!disk_data || !result || track_count == 0) return -1;
    
    // Expected track lengths
    size_t expected_length;
    double tolerance = 0.05;  // 5%
    
    switch (platform) {
        case UFT_PLATFORM_CBM:
            expected_length = 7692;  // ~300 RPM at 250 kbps
            break;
        case UFT_PLATFORM_AMIGA:
            expected_length = 12500; // MFM DD
            break;
        default:
            expected_length = disk_size / track_count;
    }
    
    size_t track_size = disk_size / track_count;
    double variance = fabs((double)track_size - expected_length) / expected_length;
    
    if (variance > tolerance) {
        if (result->track_variance_count < 84) {
            result->track_variance[result->track_variance_count].track = 0;
            result->track_variance[result->track_variance_count].expected_length = expected_length;
            result->track_variance[result->track_variance_count].actual_length = track_size;
            result->track_variance[result->track_variance_count].variance_percent = variance * 100;
            result->track_variance_count++;
        }
        result->techniques |= UFT_PROT_TECH_TRACK_LENGTH;
    }
    
    return 0;
}

// ============================================================================
// FINGERPRINT MATCHING
// ============================================================================

int uft_protection_match_fingerprint(const uint8_t* track_data,
                                     size_t track_size,
                                     int track_num,
                                     uft_prot_scheme_t* schemes,
                                     double* confidences,
                                     int max_matches) {
    if (!track_data || !schemes || !confidences || max_matches <= 0) return 0;
    
    int matches = 0;
    
    for (int i = 0; FINGERPRINTS[i].name && matches < max_matches; i++) {
        const fingerprint_entry_t* fp = &FINGERPRINTS[i];
        
        // Check track match
        if (fp->sig_track >= 0 && fp->sig_track != track_num) continue;
        
        // Search for signature
        if (fp->sig_length > 0 && track_size >= fp->sig_length) {
            for (size_t pos = 0; pos + fp->sig_length <= track_size; pos++) {
                if (memcmp(track_data + pos, fp->signature, fp->sig_length) == 0) {
                    schemes[matches] = fp->scheme;
                    confidences[matches] = 0.85;  // Base confidence
                    matches++;
                    break;
                }
            }
        }
    }
    
    return matches;
}

// ============================================================================
// MAIN DETECTION
// ============================================================================

int uft_protection_detect(const uft_protection_context_t* ctx,
                          uft_protection_result_t* result) {
    if (!ctx || !result || !ctx->data) return -1;
    
    memset(result, 0, sizeof(*result));
    result->platform = ctx->hint_platform;
    result->confidence = 0.0;
    
    // 1. Try fingerprint matching
    uft_prot_scheme_t schemes[8];
    double confidences[8];
    int matches = uft_protection_match_fingerprint(ctx->data, ctx->data_size, 
                                                   0, schemes, confidences, 8);
    
    if (matches > 0) {
        result->scheme = schemes[0];
        result->confidence = confidences[0];
    }
    
    // 2. Weak bit analysis (if multi-rev available)
    if (ctx->detect_weak_bits && ctx->revolutions && ctx->rev_count >= 2) {
        uint8_t* weak_map = calloc(1, ctx->data_size);
        if (weak_map) {
            size_t weak_count = 0;
            uft_protection_analyze_weak_bits(ctx->revolutions, ctx->rev_sizes,
                                            ctx->rev_count, weak_map, &weak_count);
            
            if (weak_count > 0) {
                result->techniques |= UFT_PROT_TECH_WEAK_BITS;
                result->weak_bit_map = weak_map;
                result->weak_bit_count = weak_count;
                result->confidence += 0.1;
            } else {
                free(weak_map);
            }
        }
    }
    
    // 3. Sync analysis
    if (ctx->detect_sync) {
        uft_protection_analyze_sync(ctx->data, ctx->data_size, 
                                   ctx->hint_platform, result);
    }
    
    // 4. Generate name/description
    if (result->scheme != UFT_PROT_SCHEME_NONE) {
        strncpy(result->name, uft_protection_scheme_name(result->scheme), 
                sizeof(result->name) - 1);
        strncpy(result->description, uft_protection_scheme_description(result->scheme),
                sizeof(result->description) - 1);
    } else if (result->techniques != UFT_PROT_TECH_NONE) {
        strncpy(result->name, "Unknown Protection", sizeof(result->name) - 1);
        snprintf(result->description, sizeof(result->description),
                "Detected techniques: 0x%08X", result->techniques);
    } else {
        strncpy(result->name, "No Protection", sizeof(result->name) - 1);
    }
    
    // Cap confidence at 1.0
    if (result->confidence > 1.0) result->confidence = 1.0;
    
    return 0;
}

// ============================================================================
// UTILITIES
// ============================================================================

const char* uft_protection_scheme_name(uft_prot_scheme_t scheme) {
    for (int i = 0; FINGERPRINTS[i].name; i++) {
        if (FINGERPRINTS[i].scheme == scheme) {
            return FINGERPRINTS[i].name;
        }
    }
    return "Unknown";
}

const char* uft_protection_scheme_description(uft_prot_scheme_t scheme) {
    for (int i = 0; FINGERPRINTS[i].name; i++) {
        if (FINGERPRINTS[i].scheme == scheme) {
            return FINGERPRINTS[i].description;
        }
    }
    return "";
}

uft_platform_t uft_protection_scheme_platform(uft_prot_scheme_t scheme) {
    for (int i = 0; FINGERPRINTS[i].name; i++) {
        if (FINGERPRINTS[i].scheme == scheme) {
            return FINGERPRINTS[i].platform;
        }
    }
    return UFT_PLATFORM_UNKNOWN;
}

const char* uft_protection_technique_name(uft_prot_technique_t tech) {
    if (tech & UFT_PROT_TECH_WEAK_BITS) return "Weak Bits";
    if (tech & UFT_PROT_TECH_FAT_TRACK) return "Fat Track";
    if (tech & UFT_PROT_TECH_HALF_TRACK) return "Half Track";
    if (tech & UFT_PROT_TECH_SYNC_ANOMALY) return "Sync Anomaly";
    if (tech & UFT_PROT_TECH_TRACK_LENGTH) return "Track Length";
    if (tech & UFT_PROT_TECH_BAD_CRC) return "Bad CRC";
    return "Unknown";
}

bool uft_protection_has_protection(const uint8_t* data, size_t size) {
    uft_protection_context_t ctx;
    uft_protection_context_init(&ctx);
    ctx.data = data;
    ctx.data_size = size;
    
    uft_protection_result_t result;
    if (uft_protection_detect(&ctx, &result) == 0) {
        return result.scheme != UFT_PROT_SCHEME_NONE || 
               result.techniques != UFT_PROT_TECH_NONE;
    }
    return false;
}

bool uft_protection_has_technique(const uft_protection_result_t* result,
                                  uft_prot_technique_t tech) {
    return result && (result->techniques & tech);
}

void uft_protection_result_free(uft_protection_result_t* result) {
    if (!result) return;
    free(result->weak_bit_map);
    memset(result, 0, sizeof(*result));
}
