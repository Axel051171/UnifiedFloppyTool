/**
 * @file uft_rapidlok.c
 * @brief Rapidlok / Half-Track Protection Scanner
 */

#include "uft_rapidlok.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal
 * ═══════════════════════════════════════════════════════════════════════════════ */

struct uft_rapidlok_scanner {
    uft_rapidlok_options_t options;
    
    /* Scan state */
    uint8_t             half_track_bitmap[42];
    int                 max_track_found;
    
    /* Statistics */
    int                 tracks_scanned;
    int                 anomalies_found;
};

/* Rapidlok signature patterns */
static const uint8_t RAPIDLOK_V1_SIG[] = { 0x52, 0x41, 0x50, 0x49, 0x44 }; /* "RAPID" */
static const uint8_t RAPIDLOK_V2_SIG[] = { 0xA9, 0x00, 0x85, 0x02 };
static const uint8_t VORPAL_SIG[] = { 0xA9, 0x0B, 0x8D, 0x00, 0x18 };

/* Standard GCR bit cell times in nanoseconds for 1541 zones */
static const double GCR_BITCELL_NS[4] = {
    3200.0,     /* Zone 0: tracks 31-35 */
    2933.0,     /* Zone 1: tracks 25-30 */
    2667.0,     /* Zone 2: tracks 18-24 */
    2500.0      /* Zone 3: tracks 1-17 */
};

static int get_zone_for_track(int track) {
    if (track >= 31) return 0;
    if (track >= 25) return 1;
    if (track >= 18) return 2;
    return 3;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_rapidlok_scanner_t* uft_rapidlok_create(const uft_rapidlok_options_t *options) {
    uft_rapidlok_scanner_t *scanner = calloc(1, sizeof(uft_rapidlok_scanner_t));
    if (!scanner) return NULL;
    
    if (options) {
        scanner->options = *options;
    } else {
        scanner->options.scan_half_tracks = true;
        scanner->options.scan_extended = true;
        scanner->options.analyze_timing = true;
        scanner->options.deep_scan = false;
        scanner->options.max_track = 42;
    }
    
    return scanner;
}

void uft_rapidlok_destroy(uft_rapidlok_scanner_t *scanner) {
    free(scanner);
}

static bool check_pattern(const uint8_t *data, size_t data_len,
                          const uint8_t *pattern, size_t pattern_len) {
    if (data_len < pattern_len) return false;
    
    for (size_t i = 0; i + pattern_len <= data_len; i++) {
        if (memcmp(data + i, pattern, pattern_len) == 0) {
            return true;
        }
    }
    return false;
}

static double analyze_timing_deviation(const double *flux_data, size_t count, int track) {
    if (count < 10) return 0.0;
    
    int zone = get_zone_for_track(track);
    double expected = GCR_BITCELL_NS[zone];
    
    double sum_deviation = 0.0;
    int valid_samples = 0;
    
    for (size_t i = 0; i < count; i++) {
        double cells = flux_data[i] / expected;
        int cell_count = (int)(cells + 0.5);
        if (cell_count >= 1 && cell_count <= 4) {
            double deviation = fabs(flux_data[i] - (cell_count * expected)) / expected;
            sum_deviation += deviation;
            valid_samples++;
        }
    }
    
    return valid_samples > 0 ? sum_deviation / valid_samples : 0.0;
}

int uft_rapidlok_scan_flux(uft_rapidlok_scanner_t *scanner,
                           const double *flux_data, size_t flux_count,
                           double track,
                           uft_c64_protection_result_t *result) {
    if (!scanner || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    scanner->tracks_scanned++;
    
    /* Check if this is a half-track */
    if (uft_is_half_track(track)) {
        result->has_half_tracks = true;
        int idx = (int)(track * 2) / 2;
        if (idx < 42) {
            scanner->half_track_bitmap[idx] |= (1 << ((int)(track * 2) % 8));
        }
        result->half_track_count++;
        result->detected = true;
        result->type = UFT_PROT_HALF_TRACK;
    }
    
    /* Check for extended tracks */
    if (track > 35.0) {
        result->has_extended_tracks = true;
        result->max_track = (int)track;
        if ((int)track > scanner->max_track_found) {
            scanner->max_track_found = (int)track;
        }
        result->extended_track_count++;
        result->detected = true;
        result->type = UFT_PROT_TRACK_36_PLUS;
    }
    
    /* Analyze timing */
    if (scanner->options.analyze_timing && flux_data && flux_count > 0) {
        double deviation = analyze_timing_deviation(flux_data, flux_count, (int)track);
        
        if (deviation > 0.15) {  /* More than 15% deviation */
            result->has_timing_anomaly = true;
            result->timing_deviation = deviation * 100.0;
            result->detected = true;
            
            if (result->type == UFT_PROT_NONE) {
                result->type = UFT_PROT_GCR_TIMING;
            }
        }
    }
    
    /* Calculate confidence */
    if (result->detected) {
        result->confidence = 0.5;
        if (result->has_half_tracks) result->confidence += 0.2;
        if (result->has_extended_tracks) result->confidence += 0.15;
        if (result->has_timing_anomaly) result->confidence += 0.15;
        if (result->confidence > 1.0) result->confidence = 1.0;
    }
    
    return 0;
}

int uft_rapidlok_scan_gcr(uft_rapidlok_scanner_t *scanner,
                          const uint8_t *gcr_data, size_t size,
                          int track,
                          uft_c64_protection_result_t *result) {
    if (!scanner || !gcr_data || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    scanner->tracks_scanned++;
    
    /* Check for Rapidlok signatures */
    if (check_pattern(gcr_data, size, RAPIDLOK_V1_SIG, sizeof(RAPIDLOK_V1_SIG))) {
        result->detected = true;
        result->type = UFT_PROT_RAPIDLOK;
        result->rapidlok_version = 1;
        result->confidence = 0.95;
        result->key_track = track;
        snprintf(result->signature, sizeof(result->signature), "Rapidlok v1");
        return 0;
    }
    
    if (check_pattern(gcr_data, size, RAPIDLOK_V2_SIG, sizeof(RAPIDLOK_V2_SIG))) {
        result->detected = true;
        result->type = UFT_PROT_RAPIDLOK;
        result->rapidlok_version = 2;
        result->confidence = 0.90;
        result->key_track = track;
        snprintf(result->signature, sizeof(result->signature), "Rapidlok v2");
        return 0;
    }
    
    if (check_pattern(gcr_data, size, VORPAL_SIG, sizeof(VORPAL_SIG))) {
        result->detected = true;
        result->type = UFT_PROT_VORPAL;
        result->confidence = 0.85;
        result->key_track = track;
        snprintf(result->signature, sizeof(result->signature), "Vorpal");
        return 0;
    }
    
    /* Check for extended track indicator */
    if (track > 35) {
        result->detected = true;
        result->type = UFT_PROT_TRACK_36_PLUS;
        result->has_extended_tracks = true;
        result->max_track = track;
        result->confidence = 0.70;
        snprintf(result->signature, sizeof(result->signature), "Extended Track %d", track);
    }
    
    return 0;
}

int uft_rapidlok_scan_disk(uft_rapidlok_scanner_t *scanner,
                           const char *image_path,
                           uft_c64_protection_result_t *result) {
    if (!scanner || !image_path || !result) return -1;
    
    /* Would read disk image and scan all tracks */
    /* For now, placeholder */
    
    memset(result, 0, sizeof(*result));
    result->confidence = 0.0;
    
    return 0;
}

const char* uft_c64_protection_name(uft_c64_protection_t type) {
    switch (type) {
        case UFT_PROT_NONE: return "None";
        case UFT_PROT_RAPIDLOK: return "Rapidlok";
        case UFT_PROT_RAPIDLOK_PLUS: return "Rapidlok+";
        case UFT_PROT_VORPAL: return "Vorpal";
        case UFT_PROT_V_MAX: return "V-Max";
        case UFT_PROT_GCR_TIMING: return "GCR Timing";
        case UFT_PROT_HALF_TRACK: return "Half-Track";
        case UFT_PROT_TRACK_36_PLUS: return "Track 36+";
        case UFT_PROT_FAT_TRACK: return "Fat Track";
        case UFT_PROT_SYNC_MARK: return "Sync Mark";
        case UFT_PROT_UNKNOWN: return "Unknown";
        default: return "Invalid";
    }
}
