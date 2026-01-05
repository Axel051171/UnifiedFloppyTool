/**
 * @file uft_c64_protection_enhanced.c
 * @brief Enhanced C64 Protection Detection Implementation
 */

#include "uft/protection/uft_c64_protection_enhanced.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

/* 1541 speed zones (tracks 1-35) */
static const uint8_t ZONE_BOUNDARIES[] = { 1, 18, 25, 31, 36 };
static const uint32_t ZONE_BITRATES[] = { 307692, 285714, 266667, 250000 };
static const uint16_t ZONE_SECTORS[] = { 21, 19, 18, 17 };

/* V-MAX! signatures */
static const uint8_t VMAX_SIG_V1[] = { 0x4C, 0x00, 0x04 };
static const uint8_t VMAX_SIG_V2[] = { 0x4C, 0x00, 0x05 };
static const uint8_t VMAX_SIG_V3[] = { 0x4C, 0x00, 0x06 };
static const uint8_t VMAX_SYNC_PATTERN[] = { 0xFF, 0xFF, 0x52, 0x54 };

/* RapidLok signatures */
static const uint8_t RAPIDLOK_HEADER[] = { 0x52, 0x4C };
static const uint8_t RAPIDLOK_V3_SIG[] = { 0xA9, 0x00, 0x8D };

/* Vorpal signatures */
static const uint8_t VORPAL_HEADER[] = { 0x56, 0x50 };
static const uint8_t VORPAL_SYNC[] = { 0xFF, 0xFF, 0x56, 0x50, 0x00 };

/*===========================================================================
 * Speed Zone Helpers
 *===========================================================================*/

uint8_t uft_c64_get_speed_zone(uint8_t track)
{
    if (track < 1) return 0;
    if (track <= 17) return 0;  /* Zone 0: Tracks 1-17 */
    if (track <= 24) return 1;  /* Zone 1: Tracks 18-24 */
    if (track <= 30) return 2;  /* Zone 2: Tracks 25-30 */
    return 3;                    /* Zone 3: Tracks 31-35+ */
}

uint32_t uft_c64_get_zone_bitrate(uint8_t zone)
{
    if (zone > 3) zone = 3;
    return ZONE_BITRATES[zone];
}

/*===========================================================================
 * V-MAX! Detection
 *===========================================================================*/

/**
 * @brief Check for V-MAX! density patterns
 */
static bool check_vmax_density(const uint32_t *intervals, size_t count,
                               uint8_t *zones, float *variance)
{
    if (!intervals || count < 1000) return false;
    
    /* Calculate interval histogram */
    uint32_t hist[64] = {0};
    float sum = 0, sum_sq = 0;
    
    for (size_t i = 0; i < count; i++) {
        uint32_t interval = intervals[i];
        sum += interval;
        sum_sq += (float)interval * interval;
        
        /* Bin to histogram (0.5µs bins) */
        uint32_t bin = interval / 500;  /* Assuming nanoseconds */
        if (bin < 64) hist[bin]++;
    }
    
    float mean = sum / count;
    *variance = (sum_sq / count) - (mean * mean);
    
    /* V-MAX! typically has 4 distinct density zones */
    int peaks = 0;
    for (int i = 1; i < 63; i++) {
        if (hist[i] > hist[i-1] && hist[i] > hist[i+1] && hist[i] > count/50) {
            if (peaks < 4) {
                zones[peaks] = i;
            }
            peaks++;
        }
    }
    
    return peaks >= 3;
}

int uft_c64_detect_vmax(const uint8_t *gcr_data, size_t size,
                        uint8_t track, const uft_vmax_params_t *params,
                        uft_vmax_result_t *result)
{
    if (!gcr_data || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    float confidence = 0.0f;
    
    /* Search for V-MAX! loader signatures */
    for (size_t i = 0; i < size - 3; i++) {
        if (memcmp(gcr_data + i, VMAX_SIG_V3, 3) == 0) {
            result->version = UFT_VMAX_V3;
            confidence += 0.4f;
            memcpy(result->loader_sig, gcr_data + i, 16);
            result->loader_addr = 0x0600;
            break;
        }
        if (memcmp(gcr_data + i, VMAX_SIG_V2, 3) == 0) {
            result->version = UFT_VMAX_V2;
            confidence += 0.35f;
            memcpy(result->loader_sig, gcr_data + i, 16);
            result->loader_addr = 0x0500;
            break;
        }
        if (memcmp(gcr_data + i, VMAX_SIG_V1, 3) == 0) {
            result->version = UFT_VMAX_V1;
            confidence += 0.3f;
            memcpy(result->loader_sig, gcr_data + i, 16);
            result->loader_addr = 0x0400;
            break;
        }
    }
    
    /* Search for V-MAX! sync patterns */
    if (params && params->check_sync_patterns) {
        for (size_t i = 0; i < size - 4; i++) {
            if (memcmp(gcr_data + i, VMAX_SYNC_PATTERN, 4) == 0) {
                memcpy(result->sync_pattern, gcr_data + i, 8);
                result->sync_length = 4;
                confidence += 0.25f;
                break;
            }
        }
    }
    
    /* Track 36+ is common for V-MAX! protection */
    if (track >= 36) {
        result->protection_track = track;
        confidence += 0.2f;
    }
    
    /* Set detection result */
    result->confidence = confidence;
    result->detected = confidence >= (params ? params->min_confidence : 0.5f);
    
    return 0;
}

int uft_vmax_analyze_density(const uint32_t *flux_intervals, size_t count,
                             uint8_t *zones, float *variance)
{
    if (!flux_intervals || !zones || !variance) return -1;
    
    if (check_vmax_density(flux_intervals, count, zones, variance)) {
        return 0;
    }
    
    return -1;
}

int uft_vmax_decode_sector(const uint8_t *gcr_data, size_t size,
                           uft_vmax_version_t version,
                           uint8_t *decoded, size_t *decoded_size)
{
    if (!gcr_data || !decoded || !decoded_size) return -1;
    
    /*
     * V-MAX! GCR Decoding
     * 
     * V-MAX! uses custom GCR tables that differ by version.
     * Standard C64 GCR: 4 bits → 5 bits (with clock bits)
     * V-MAX! uses scrambled tables to prevent copying.
     * 
     * V-MAX! versions:
     * - V1 (early): Simple table scramble
     * - V2: More complex scrambling
     * - V3: Density variation + scrambling
     */
    
    /* Standard C64 GCR decode table (5-bit → 4-bit) */
    static const int8_t GCR_DECODE_STD[32] = {
        -1, -1, -1, -1, -1, -1, -1, -1,
        -1,  8,  0,  1, -1, 12,  4,  5,
        -1, -1,  2,  3, -1, 15,  6,  7,
        -1,  9, 10, 11, -1, 13, 14, -1
    };
    
    /* V-MAX! scrambled decode tables */
    static const int8_t VMAX_DECODE_V1[32] = {
        -1, -1, -1, -1, -1, -1, -1, -1,
        -1,  9,  1,  0, -1, 13,  5,  4,
        -1, -1,  3,  2, -1, 14,  7,  6,
        -1,  8, 11, 10, -1, 12, 15, -1
    };
    
    static const int8_t VMAX_DECODE_V2[32] = {
        -1, -1, -1, -1, -1, -1, -1, -1,
        -1, 10,  2,  1, -1, 14,  6,  5,
        -1, -1,  0,  3, -1, 12,  4,  7,
        -1,  9,  8, 11, -1, 13, 15, -1
    };
    
    static const int8_t VMAX_DECODE_V3[32] = {
        -1, -1, -1, -1, -1, -1, -1, -1,
        -1, 11,  3,  0, -1, 15,  7,  4,
        -1, -1,  1,  2, -1, 13,  5,  6,
        -1,  8,  9, 10, -1, 12, 14, -1
    };
    
    /* Select decode table based on version */
    const int8_t *decode_table;
    switch (version) {
        case UFT_VMAX_V1:
            decode_table = VMAX_DECODE_V1;
            break;
        case UFT_VMAX_V2:
            decode_table = VMAX_DECODE_V2;
            break;
        case UFT_VMAX_V3:
            decode_table = VMAX_DECODE_V3;
            break;
        default:
            decode_table = GCR_DECODE_STD;
            break;
    }
    
    /* GCR decodes 5 bits → 4 bits (8 bytes GCR → 5 bytes data) */
    size_t out_size = (size * 4) / 5;
    if (*decoded_size < out_size) {
        *decoded_size = out_size;
        return -1;
    }
    
    /* Decode GCR data */
    size_t in_pos = 0;
    size_t out_pos = 0;
    uint32_t buffer = 0;
    int bits = 0;
    int errors = 0;
    
    while (in_pos < size && out_pos < out_size) {
        /* Accumulate bits */
        while (bits < 10 && in_pos < size) {
            buffer = (buffer << 8) | gcr_data[in_pos++];
            bits += 8;
        }
        
        /* Extract two 5-bit GCR nibbles */
        if (bits >= 10) {
            int shift = bits - 10;
            uint8_t gcr1 = (buffer >> (shift + 5)) & 0x1F;
            uint8_t gcr2 = (buffer >> shift) & 0x1F;
            
            int8_t nib1 = decode_table[gcr1];
            int8_t nib2 = decode_table[gcr2];
            
            if (nib1 < 0 || nib2 < 0) {
                /* Invalid GCR - use 0 and mark error */
                decoded[out_pos++] = 0x00;
                errors++;
            } else {
                decoded[out_pos++] = (nib1 << 4) | nib2;
            }
            
            bits -= 10;
            buffer &= (1u << bits) - 1;
        }
    }
    
    *decoded_size = out_pos;
    
    /* Return number of errors, 0 = success */
    return errors;
}

/*===========================================================================
 * RapidLok Detection
 *===========================================================================*/

int uft_c64_detect_rapidlok(const uint8_t *gcr_data, size_t size,
                            uint8_t track, uft_rapidlok_result_t *result)
{
    if (!gcr_data || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    float confidence = 0.0f;
    
    /* Search for RapidLok header signature */
    for (size_t i = 0; i < size - 2; i++) {
        if (memcmp(gcr_data + i, RAPIDLOK_HEADER, 2) == 0) {
            memcpy(result->header_sig, gcr_data + i, 4);
            confidence += 0.3f;
            break;
        }
    }
    
    /* Check for V3 signature */
    for (size_t i = 0; i < size - 3; i++) {
        if (memcmp(gcr_data + i, RAPIDLOK_V3_SIG, 3) == 0) {
            result->version = UFT_RAPIDLOK_V3;
            confidence += 0.25f;
            break;
        }
    }
    
    /* RapidLok uses track 18 for key storage */
    if (track == 18) {
        result->key_track = 18;
        confidence += 0.2f;
    }
    
    /* Check for non-standard sector count */
    /* RapidLok often uses 20 sectors instead of 19 on track 18 */
    uint8_t zone = uft_c64_get_speed_zone(track);
    if (zone == 1 && size > 7500) {  /* Larger than normal */
        result->sectors_per_track = 20;
        confidence += 0.15f;
    }
    
    /* Timing protection check */
    result->has_timing_check = true;
    result->timing_window_us = 2000;  /* 2ms window typical */
    
    result->confidence = confidence;
    result->detected = confidence >= 0.5f;
    
    return 0;
}

int uft_rapidlok_extract_key(const uint8_t *key_sector, size_t size,
                             uint32_t *seed, uint8_t *key, size_t key_size)
{
    if (!key_sector || !seed || !key) return -1;
    if (size < 256 || key_size < 256) return -1;
    
    /* RapidLok key is typically in first 256 bytes */
    /* Seed is at offset 0x10 */
    *seed = (key_sector[0x10] << 24) | (key_sector[0x11] << 16) |
            (key_sector[0x12] << 8) | key_sector[0x13];
    
    /* Key table starts at offset 0x20 */
    memcpy(key, key_sector + 0x20, key_size > 224 ? 224 : key_size);
    
    return 0;
}

int uft_rapidlok_decrypt(const uint8_t *encrypted, size_t size,
                         const uint8_t *key, size_t key_size,
                         uint8_t *decrypted)
{
    if (!encrypted || !key || !decrypted) return -1;
    
    /* RapidLok uses XOR-based encryption with rotating key */
    for (size_t i = 0; i < size; i++) {
        decrypted[i] = encrypted[i] ^ key[i % key_size];
    }
    
    return 0;
}

/*===========================================================================
 * Vorpal Detection
 *===========================================================================*/

int uft_c64_detect_vorpal(const uint8_t *gcr_data, size_t size,
                          uint8_t track, uft_vorpal_result_t *result)
{
    if (!gcr_data || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    float confidence = 0.0f;
    
    /* Search for Vorpal header */
    for (size_t i = 0; i < size - 2; i++) {
        if (memcmp(gcr_data + i, VORPAL_HEADER, 2) == 0) {
            confidence += 0.3f;
            break;
        }
    }
    
    /* Search for Vorpal sync pattern */
    for (size_t i = 0; i < size - 5; i++) {
        if (memcmp(gcr_data + i, VORPAL_SYNC, 5) == 0) {
            memcpy(result->header_sync, gcr_data + i, 5);
            confidence += 0.25f;
            break;
        }
    }
    
    /* Vorpal typically uses 19 logical sectors */
    result->logical_sectors = 19;
    result->physical_size = 336;  /* Slightly larger than standard */
    
    /* Check for custom GCR markers */
    int custom_markers = 0;
    for (size_t i = 0; i < size - 1; i++) {
        /* Vorpal uses non-standard GCR bytes */
        if (gcr_data[i] == 0x55 && gcr_data[i+1] == 0xAA) {
            custom_markers++;
        }
    }
    if (custom_markers >= 3) {
        result->uses_custom_gcr = true;
        confidence += 0.2f;
    }
    
    /* Determine Vorpal type */
    if (confidence > 0.6f) {
        result->type = UFT_VORPAL_ENHANCED;
    } else if (confidence > 0.4f) {
        result->type = UFT_VORPAL_STANDARD;
    }
    
    result->confidence = confidence;
    result->detected = confidence >= 0.4f;
    
    (void)track;
    return 0;
}

int uft_vorpal_decode(const uint8_t *gcr_data, size_t size,
                      const uint8_t *gcr_table,
                      uint8_t *decoded, size_t *decoded_size)
{
    if (!gcr_data || !decoded || !decoded_size) return -1;
    
    /* Use default GCR table if none provided */
    static const uint8_t DEFAULT_GCR[] = {
        0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
        0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
    };
    
    const uint8_t *table = gcr_table ? gcr_table : DEFAULT_GCR;
    
    /* GCR decode (5 bytes → 4 bytes) */
    size_t out_idx = 0;
    for (size_t i = 0; i + 5 <= size && out_idx + 4 <= *decoded_size; i += 5) {
        uint64_t gcr = 0;
        for (int j = 0; j < 5; j++) {
            gcr = (gcr << 8) | gcr_data[i + j];
        }
        
        /* Extract 4 nybbles */
        for (int j = 3; j >= 0; j--) {
            uint8_t nybble = (gcr >> (j * 10 + 5)) & 0x1F;
            /* Find in table */
            for (int k = 0; k < 16; k++) {
                if (table[k] == nybble) {
                    if (j % 2 == 1) {
                        decoded[out_idx] = k << 4;
                    } else {
                        decoded[out_idx++] |= k;
                    }
                    break;
                }
            }
        }
    }
    
    *decoded_size = out_idx;
    return 0;
}

/*===========================================================================
 * Fat Track Detection
 *===========================================================================*/

int uft_c64_detect_fat_track(const uint32_t *flux_data, size_t flux_count,
                             uint8_t track, uint8_t half_track,
                             uft_fat_track_result_t *result)
{
    if (!flux_data || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    result->track_number = track;
    result->half_track = half_track;
    
    /* Calculate expected track size */
    uint8_t zone = uft_c64_get_speed_zone(track);
    uint32_t expected_bits = ZONE_SECTORS[zone] * 256 * 8;  /* sectors * bytes * bits */
    uint32_t expected_flux = expected_bits;  /* Approximately */
    
    result->standard_size = expected_flux;
    result->actual_size = flux_count;
    result->size_ratio = (float)flux_count / expected_flux;
    
    /* Calculate average interval */
    uint64_t sum = 0;
    for (size_t i = 0; i < flux_count; i++) {
        sum += flux_data[i];
    }
    result->avg_interval_us = (float)sum / flux_count / 1000.0f;  /* Assuming ns input */
    
    /* Density factor (relative to zone 0) */
    result->density_factor = result->size_ratio;
    
    /* Fat track detection: >10% larger than expected */
    result->detected = result->size_ratio > 1.1f;
    result->confidence = result->size_ratio > 1.2f ? 0.9f :
                        result->size_ratio > 1.1f ? 0.7f : 0.3f;
    
    /* Check if likely copy protection */
    result->is_copy_protection = result->detected && track > 35;
    
    result->flux_count = flux_count;
    
    return 0;
}

int uft_c64_scan_fat_tracks(const void *disk_image,
                            uft_fat_track_result_t *results,
                            size_t max_results, size_t *found)
{
    if (!disk_image || !results || !found) return -1;
    
    /* This would scan all tracks in the disk image */
    /* Placeholder implementation */
    *found = 0;
    
    return 0;
}

/*===========================================================================
 * GCR Timing Analysis
 *===========================================================================*/

int uft_c64_analyze_gcr_timing(const uint32_t *flux_intervals, size_t count,
                               uint8_t speed_zone,
                               uft_gcr_timing_result_t *result)
{
    if (!flux_intervals || !result) return -1;
    if (count < 100) return -1;
    
    memset(result, 0, sizeof(*result));
    
    /* Calculate statistics */
    double sum = 0, sum_sq = 0;
    uint32_t min_val = UINT32_MAX, max_val = 0;
    
    for (size_t i = 0; i < count; i++) {
        uint32_t interval = flux_intervals[i];
        sum += interval;
        sum_sq += (double)interval * interval;
        if (interval < min_val) min_val = interval;
        if (interval > max_val) max_val = interval;
        
        /* Build histogram (0.1µs bins, assuming nanosecond input) */
        uint32_t bin = interval / 100;
        if (bin < 256) result->histogram[bin]++;
    }
    
    result->mean_interval_us = (float)(sum / count / 1000.0);
    result->std_deviation_us = (float)sqrt((sum_sq / count) - 
                                           (sum / count) * (sum / count)) / 1000.0f;
    result->min_interval_us = min_val / 1000.0f;
    result->max_interval_us = max_val / 1000.0f;
    
    /* Calculate expected timing windows based on zone */
    /* 1541 uses ~26µs bit cells in zone 0 */
    float base_cell = 26.0f / (speed_zone + 1) * 0.8f;  /* Approximate */
    result->window_1_center = base_cell * 1.0f;
    result->window_2_center = base_cell * 1.5f;
    result->window_3_center = base_cell * 2.0f;
    result->window_4_center = base_cell * 2.5f;
    
    /* Classify intervals */
    for (size_t i = 0; i < count; i++) {
        float interval_us = flux_intervals[i] / 1000.0f;
        if (interval_us < result->window_1_center * 1.25f) {
            result->short_bits++;
        } else if (interval_us < result->window_3_center * 1.25f) {
            result->normal_bits++;
        } else {
            result->long_bits++;
        }
    }
    
    /* Detect anomalies */
    float short_ratio = (float)result->short_bits / count;
    float long_ratio = (float)result->long_bits / count;
    
    result->has_non_standard_timing = (short_ratio > 0.1f) || (long_ratio > 0.1f);
    result->has_weak_bits = result->std_deviation_us > result->mean_interval_us * 0.3f;
    
    /* Check for density shift (bimodal distribution) */
    int peaks = 0;
    for (int i = 5; i < 250; i++) {
        if (result->histogram[i] > result->histogram[i-1] &&
            result->histogram[i] > result->histogram[i+1] &&
            result->histogram[i] > count / 100) {
            peaks++;
        }
    }
    result->has_density_shift = peaks > 4;
    
    result->anomaly_detected = result->has_non_standard_timing ||
                               result->has_weak_bits ||
                               result->has_density_shift;
    result->confidence = result->anomaly_detected ? 0.8f : 0.3f;
    
    return 0;
}

int uft_c64_detect_timing_protection(const uint32_t *flux_intervals, size_t count,
                                     uint8_t track, float *confidence,
                                     char *protection_name, size_t name_size)
{
    if (!flux_intervals || !confidence || !protection_name) return -1;
    
    uft_gcr_timing_result_t timing;
    uint8_t zone = uft_c64_get_speed_zone(track);
    
    if (uft_c64_analyze_gcr_timing(flux_intervals, count, zone, &timing) != 0) {
        return -1;
    }
    
    *confidence = timing.confidence;
    
    if (timing.has_density_shift) {
        strncpy(protection_name, "V-MAX!/Density", name_size - 1);
    } else if (timing.has_weak_bits) {
        strncpy(protection_name, "Weak Bits", name_size - 1);
    } else if (timing.has_non_standard_timing) {
        strncpy(protection_name, "Timing Protection", name_size - 1);
    } else {
        strncpy(protection_name, "None", name_size - 1);
    }
    protection_name[name_size - 1] = '\0';
    
    return 0;
}

/*===========================================================================
 * Unified Scanner
 *===========================================================================*/

int uft_c64_scan_all_protection(const void *disk_image,
                                uft_c64_protection_scan_t *result)
{
    if (!disk_image || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    /* This would iterate through all tracks and run all detectors */
    /* Placeholder - actual implementation would:
       1. Read each track from disk image
       2. Run V-MAX!, RapidLok, Vorpal detection
       3. Check for fat tracks
       4. Analyze GCR timing
       5. Compile results */
    
    result->has_protection = false;
    strncpy(result->primary_protection, "None", 31);
    result->overall_confidence = 0.0f;
    
    return 0;
}

int uft_c64_protection_report_json(const uft_c64_protection_scan_t *scan,
                                   char *buffer, size_t size)
{
    if (!scan || !buffer || size == 0) return -1;
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"has_protection\": %s,\n"
        "  \"primary_protection\": \"%s\",\n"
        "  \"confidence\": %.4f,\n"
        "  \"vmax\": { \"detected\": %s, \"version\": %d, \"confidence\": %.4f },\n"
        "  \"rapidlok\": { \"detected\": %s, \"version\": %d, \"confidence\": %.4f },\n"
        "  \"vorpal\": { \"detected\": %s, \"type\": %d, \"confidence\": %.4f },\n"
        "  \"fat_tracks\": %d,\n"
        "  \"protected_tracks\": %d,\n"
        "  \"unreadable_sectors\": %d\n"
        "}",
        scan->has_protection ? "true" : "false",
        scan->primary_protection,
        scan->overall_confidence,
        scan->vmax.detected ? "true" : "false",
        scan->vmax.version,
        scan->vmax.confidence,
        scan->rapidlok.detected ? "true" : "false",
        scan->rapidlok.version,
        scan->rapidlok.confidence,
        scan->vorpal.detected ? "true" : "false",
        scan->vorpal.type,
        scan->vorpal.confidence,
        scan->fat_track_count,
        scan->protected_tracks,
        scan->unreadable_sectors
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}
