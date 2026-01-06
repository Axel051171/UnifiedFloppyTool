/**
 * @file uft_protection_detect.c
 * @brief Copy Protection Detection Implementation
 * 
 * Comprehensive copy protection detection for:
 * - C64: V-MAX, PirateSlayer, RapidLok, Fat Tracks
 * - Amiga: CopyLock, Speedlock, Psygnosis, Long Tracks
 * - Generic: Weak bits, Fuzzy bits, Extra/Missing sectors
 * 
 * @author UFT Team
 * @version 3.6.0
 * @date 2026-01-03
 * 
 * SPDX-License-Identifier: MIT
 */

#include "uft/uft_protection_detect.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================================================
 * Signature Tables
 *============================================================================*/

/* V-MAX duplicator markers */
const uint8_t UFT_VMAX_MARKERS[5] = {0xA5, 0x1E, 0x78, 0xE1, 0x87};

/* Cinemaware V-MAX marker (bit-shifted) */
const uint8_t UFT_VMAX_CW_MARKER[4] = {0x4B, 0x3C, 0xF0, 0xC3};

/* PirateSlayer signature v1 */
const uint8_t UFT_PIRATESLAYER_SIG_V1[5] = {0x07, 0x07, 0xFC, 0xFC, 0x01};

/* PirateSlayer signature v2 */
const uint8_t UFT_PIRATESLAYER_SIG_V2[4] = {0x87, 0x07, 0xFC, 0xFE};

/* Amiga DOS sync words */
const uint32_t UFT_AMIGA_DOS_SYNCS[4] = {
    0x44894489,  /* Standard AmigaDOS */
    0x44894489,
    0x44894489,
    0x44894489
};

/* CopyLock non-standard sync words */
const uint16_t UFT_COPYLOCK_SYNCS[11] = {
    0x4891, 0x4A91, 0x4489, 0x4A89, 0x4291,
    0x4494, 0x4A94, 0x4524, 0x4A24, 0x4522, 0x4A22
};

/* Long track lengths (bits) indicating protection */
const uint32_t UFT_AMIGA_LONG_TRACKS[7] = {
    105500,  /* Psygnosis */
    109300,  /* CopyLock */
    110000,  /* Various */
    111000,  /* Speedlock */
    112000,  /* Rob Northen */
    115000,  /* Factor5 */
    118000   /* Extreme */
};

/* Speedlock default parameters */
const uft_speedlock_params_t UFT_SPEEDLOCK_DEFAULT = {
    .offset_bytes = 9756,
    .offset_bits = 78048,
    .long_bytes = 120,
    .short_bytes = 120,
    .timing_variation_pct = 10.0f,
    .ewma_tick_us = 0.2f,
    .threshold_ticks = 8
};

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * @brief Search for pattern in buffer
 */
static const uint8_t *find_pattern(const uint8_t *data, size_t data_len,
                                    const uint8_t *pattern, size_t pattern_len) {
    if (!data || !pattern || data_len < pattern_len) return NULL;
    
    for (size_t i = 0; i <= data_len - pattern_len; i++) {
        if (memcmp(data + i, pattern, pattern_len) == 0) {
            return data + i;
        }
    }
    return NULL;
}

/**
 * @brief Search with bit shift
 */
static const uint8_t *find_pattern_shifted(const uint8_t *data, size_t data_len,
                                            const uint8_t *pattern, size_t pattern_len,
                                            int *shift_out) {
    if (!data || !pattern || data_len < pattern_len + 1) return NULL;
    
    /* Try all 8 bit alignments */
    for (int shift = 0; shift < 8; shift++) {
        for (size_t i = 0; i < data_len - pattern_len; i++) {
            bool match = true;
            
            for (size_t j = 0; j < pattern_len && match; j++) {
                uint8_t byte = (data[i + j] << shift);
                if (i + j + 1 < data_len) {
                    byte |= (data[i + j + 1] >> (8 - shift));
                }
                if (byte != pattern[j]) {
                    match = false;
                }
            }
            
            if (match) {
                if (shift_out) *shift_out = shift;
                return data + i;
            }
        }
    }
    return NULL;
}

/**
 * @brief Count consecutive bytes
 */
static size_t count_consecutive(const uint8_t *data, size_t max_len, uint8_t value) {
    size_t count = 0;
    while (count < max_len && data[count] == value) {
        count++;
    }
    return count;
}

/*============================================================================
 * C64 Protection Detection
 *============================================================================*/

const uint8_t *uft_prot_detect_vmax(const uint8_t *track_data,
                                    size_t track_len,
                                    uft_protection_result_t *result) {
    if (!track_data || track_len == 0) return NULL;
    
    const uint8_t *pos = find_pattern(track_data, track_len,
                                       UFT_VMAX_MARKERS, sizeof(UFT_VMAX_MARKERS));
    
    if (pos && result) {
        result->type = UFT_PROT_VMAX;
        result->name = "V-MAX";
        result->family = "Vorpal";
        result->confidence = 95;
        result->offset = pos - track_data;
        memcpy(result->signature, UFT_VMAX_MARKERS, sizeof(UFT_VMAX_MARKERS));
        result->signature_len = sizeof(UFT_VMAX_MARKERS);
        result->align_point = pos;
        snprintf(result->notes, sizeof(result->notes),
                 "V-MAX duplicator protection at offset 0x%zX", result->offset);
    }
    
    return pos;
}

const uint8_t *uft_prot_detect_vmax_cw(const uint8_t *track_data,
                                       size_t track_len,
                                       uft_protection_result_t *result) {
    if (!track_data || track_len == 0) return NULL;
    
    int shift;
    const uint8_t *pos = find_pattern_shifted(track_data, track_len,
                                               UFT_VMAX_CW_MARKER, 
                                               sizeof(UFT_VMAX_CW_MARKER),
                                               &shift);
    
    if (pos && result) {
        result->type = UFT_PROT_VMAX_CW;
        result->name = "V-MAX Cinemaware";
        result->family = "Vorpal";
        result->confidence = 90;
        result->offset = pos - track_data;
        memcpy(result->signature, UFT_VMAX_CW_MARKER, sizeof(UFT_VMAX_CW_MARKER));
        result->signature_len = sizeof(UFT_VMAX_CW_MARKER);
        result->align_point = pos;
        snprintf(result->notes, sizeof(result->notes),
                 "V-MAX Cinemaware variant at offset 0x%zX (shift=%d)", 
                 result->offset, shift);
    }
    
    return pos;
}

const uint8_t *uft_prot_detect_pirateslayer(const uint8_t *track_data,
                                            size_t track_len,
                                            uft_protection_result_t *result) {
    if (!track_data || track_len == 0) return NULL;
    
    const uint8_t *pos = NULL;
    int shift = 0;
    
    /* Try v1 signature */
    pos = find_pattern_shifted(track_data, track_len,
                               UFT_PIRATESLAYER_SIG_V1, 
                               sizeof(UFT_PIRATESLAYER_SIG_V1),
                               &shift);
    
    if (pos && result) {
        result->type = UFT_PROT_PIRATESLAYER;
        result->name = "PirateSlayer";
        result->family = "EA/Activision";
        result->confidence = 90;
        result->offset = pos - track_data;
        memcpy(result->signature, UFT_PIRATESLAYER_SIG_V1, 
               sizeof(UFT_PIRATESLAYER_SIG_V1));
        result->signature_len = sizeof(UFT_PIRATESLAYER_SIG_V1);
        snprintf(result->notes, sizeof(result->notes),
                 "PirateSlayer v1 at offset 0x%zX (shift=%d)", 
                 result->offset, shift);
        return pos;
    }
    
    /* Try v2 signature */
    pos = find_pattern_shifted(track_data, track_len,
                               UFT_PIRATESLAYER_SIG_V2, 
                               sizeof(UFT_PIRATESLAYER_SIG_V2),
                               &shift);
    
    if (pos && result) {
        result->type = UFT_PROT_PIRATESLAYER_V2;
        result->name = "PirateSlayer v2";
        result->family = "EA/Activision";
        result->confidence = 85;
        result->offset = pos - track_data;
        memcpy(result->signature, UFT_PIRATESLAYER_SIG_V2, 
               sizeof(UFT_PIRATESLAYER_SIG_V2));
        result->signature_len = sizeof(UFT_PIRATESLAYER_SIG_V2);
        snprintf(result->notes, sizeof(result->notes),
                 "PirateSlayer v2 at offset 0x%zX (shift=%d)", 
                 result->offset, shift);
    }
    
    return pos;
}

const uint8_t *uft_prot_detect_rapidlok(const uint8_t *track_data,
                                        size_t track_len,
                                        uft_protection_result_t *result) {
    if (!track_data || track_len < 200) return NULL;
    
    /* Look for RapidLok Track Header:
     * - 21+ sync bytes (0xFF)
     * - 0x55 ID byte
     * - 164+ 0x7B bytes
     */
    
    for (size_t i = 0; i < track_len - 200; i++) {
        /* Count sync bytes */
        size_t sync_count = count_consecutive(track_data + i, track_len - i, 0xFF);
        if (sync_count < 21) continue;
        
        /* Check for ID byte */
        size_t id_pos = i + sync_count;
        if (id_pos >= track_len || track_data[id_pos] != 0x55) continue;
        
        /* Count 0x7B bytes */
        size_t header_count = count_consecutive(track_data + id_pos + 1,
                                                 track_len - id_pos - 1, 0x7B);
        if (header_count >= 164) {
            if (result) {
                result->type = UFT_PROT_RAPIDLOK;
                result->name = "RapidLok";
                result->family = "Rapidlok";
                result->confidence = 95;
                result->offset = i;
                result->signature[0] = 0xFF;
                result->signature[1] = 0x55;
                result->signature[2] = 0x7B;
                result->signature_len = 3;
                result->align_point = track_data + id_pos + 1;
                snprintf(result->notes, sizeof(result->notes),
                         "RapidLok at offset 0x%zX: %zu sync, %zu header bytes",
                         i, sync_count, header_count);
            }
            return track_data + i;
        }
    }
    
    return NULL;
}

bool uft_prot_detect_fat_track(const uint8_t *track_a, size_t len_a,
                               const uint8_t *track_b, size_t len_b,
                               size_t *match_bytes) {
    if (!track_a || !track_b || len_a == 0 || len_b == 0) {
        if (match_bytes) *match_bytes = 0;
        return false;
    }
    
    /* Compare overlap region */
    size_t min_len = len_a < len_b ? len_a : len_b;
    size_t matches = 0;
    
    for (size_t i = 0; i < min_len; i++) {
        if (track_a[i] == track_b[i]) {
            matches++;
        }
    }
    
    if (match_bytes) *match_bytes = matches;
    
    /* Fat track if >80% match */
    return (matches * 100 / min_len) >= 80;
}

/*============================================================================
 * Amiga Protection Detection
 *============================================================================*/

bool uft_prot_detect_copylock(const uint8_t *track_data,
                              size_t track_len,
                              uft_protection_result_t *result) {
    if (!track_data || track_len < 4) return false;
    
    /* Search for non-standard sync words */
    for (size_t i = 0; i < track_len - 2; i++) {
        uint16_t sync = ((uint16_t)track_data[i] << 8) | track_data[i + 1];
        
        for (int s = 0; s < 11; s++) {
            if (sync == UFT_COPYLOCK_SYNCS[s]) {
                if (result) {
                    result->type = UFT_PROT_COPYLOCK;
                    result->name = "CopyLock";
                    result->family = "Rob Northen";
                    result->confidence = 85;
                    result->offset = i;
                    result->signature[0] = track_data[i];
                    result->signature[1] = track_data[i + 1];
                    result->signature_len = 2;
                    snprintf(result->notes, sizeof(result->notes),
                             "CopyLock sync 0x%04X at offset 0x%zX", sync, i);
                }
                return true;
            }
        }
    }
    
    return false;
}

bool uft_prot_detect_speedlock(const uint8_t *track_data,
                               size_t track_len,
                               const uint32_t *timing_ns,
                               size_t timing_count,
                               uft_protection_result_t *result) {
    if (!track_data || !timing_ns || timing_count == 0) return false;
    
    const uft_speedlock_params_t *params = &UFT_SPEEDLOCK_DEFAULT;
    
    /* Check if we have data at the expected offset */
    if (track_len < params->offset_bytes + 200) return false;
    
    /* Analyze timing variance at offset */
    size_t start = params->offset_bytes;
    size_t end = start + params->long_bytes + params->short_bytes;
    if (end > timing_count) return false;
    
    /* Calculate average and variance */
    double sum = 0, sum_sq = 0;
    size_t count = 0;
    
    for (size_t i = start; i < end; i++) {
        double t = timing_ns[i];
        sum += t;
        sum_sq += t * t;
        count++;
    }
    
    double mean = sum / count;
    double variance = (sum_sq / count) - (mean * mean);
    double variation_pct = (sqrt(variance) / mean) * 100.0;
    
    /* Speedlock has characteristic timing variations */
    if (variation_pct >= params->timing_variation_pct * 0.8 &&
        variation_pct <= params->timing_variation_pct * 1.2) {
        
        if (result) {
            result->type = UFT_PROT_SPEEDLOCK;
            result->name = "Speedlock";
            result->family = "Speedlock";
            result->confidence = 75;
            result->offset = params->offset_bytes;
            snprintf(result->notes, sizeof(result->notes),
                     "Speedlock timing variation %.1f%% at offset %u",
                     variation_pct, params->offset_bytes);
        }
        return true;
    }
    
    return false;
}

bool uft_prot_detect_long_track(size_t track_len,
                                uft_protection_result_t *result) {
    /* Standard Amiga track is ~100,000 bits */
    if (track_len < 104000) return false;
    
    /* Check against known long track lengths */
    for (int i = 0; i < 7; i++) {
        /* Allow 2% tolerance */
        size_t expected = UFT_AMIGA_LONG_TRACKS[i];
        size_t tolerance = expected / 50;
        
        if (track_len >= expected - tolerance && 
            track_len <= expected + tolerance) {
            
            if (result) {
                result->type = UFT_PROT_LONG_TRACK;
                result->name = "Long Track";
                result->family = "Track Length";
                result->confidence = 80;
                result->offset = 0;
                snprintf(result->notes, sizeof(result->notes),
                         "Long track: %zu bits (expected %zu)", 
                         track_len, expected);
            }
            return true;
        }
    }
    
    /* Generic long track */
    if (track_len > 104000) {
        if (result) {
            result->type = UFT_PROT_LONG_TRACK;
            result->name = "Long Track (Unknown)";
            result->family = "Track Length";
            result->confidence = 60;
            result->offset = 0;
            snprintf(result->notes, sizeof(result->notes),
                     "Non-standard long track: %zu bits", track_len);
        }
        return true;
    }
    
    return false;
}

bool uft_prot_detect_rnc_hidden(const uint8_t *track_data,
                                size_t track_len,
                                uft_protection_result_t *result) {
    if (!track_data || track_len < 100) return false;
    
    /* Count non-standard sync words */
    int non_standard_count = 0;
    
    for (size_t i = 0; i < track_len - 2; i++) {
        uint16_t sync = ((uint16_t)track_data[i] << 8) | track_data[i + 1];
        
        /* Standard sync is 0x4489 */
        if (sync != 0x4489) {
            /* Check if it's a valid sync pattern */
            if ((sync & 0xF000) == 0x4000) {
                non_standard_count++;
            }
        }
    }
    
    /* RNC hidden sectors use multiple non-standard syncs */
    if (non_standard_count >= 3) {
        if (result) {
            result->type = UFT_PROT_RNC_HIDDEN;
            result->name = "RNC Hidden Sectors";
            result->family = "Rob Northen";
            result->confidence = 70;
            result->offset = 0;
            snprintf(result->notes, sizeof(result->notes),
                     "Found %d non-standard sync words", non_standard_count);
        }
        return true;
    }
    
    return false;
}

/*============================================================================
 * Generic Protection Detection
 *============================================================================*/

bool uft_prot_detect_weak_bits(const uint8_t **reads,
                               size_t read_count,
                               size_t track_len,
                               uint8_t *weak_map,
                               size_t *weak_count) {
    if (!reads || read_count < 2 || !weak_map || !weak_count) {
        if (weak_count) *weak_count = 0;
        return false;
    }
    
    memset(weak_map, 0, track_len);
    *weak_count = 0;
    
    /* Compare all reads for inconsistencies */
    for (size_t i = 0; i < track_len; i++) {
        uint8_t ref = reads[0][i];
        bool is_weak = false;
        
        for (size_t r = 1; r < read_count; r++) {
            if (reads[r][i] != ref) {
                is_weak = true;
                break;
            }
        }
        
        if (is_weak) {
            weak_map[i] = 1;
            (*weak_count)++;
        }
    }
    
    return (*weak_count > 0);
}

bool uft_prot_detect_extra_sectors(int expected_sectors,
                                   int found_sectors,
                                   uft_protection_result_t *result) {
    if (found_sectors <= expected_sectors) return false;
    
    if (result) {
        result->type = UFT_PROT_EXTRA_SECTORS;
        result->name = "Extra Sectors";
        result->family = "Sector Count";
        result->confidence = 90;
        result->offset = 0;
        snprintf(result->notes, sizeof(result->notes),
                 "Found %d sectors, expected %d", found_sectors, expected_sectors);
    }
    return true;
}

bool uft_prot_detect_missing_sectors(int expected_sectors,
                                     const bool *sector_found,
                                     uft_protection_result_t *result) {
    if (!sector_found) return false;
    
    int missing = 0;
    for (int i = 0; i < expected_sectors; i++) {
        if (!sector_found[i]) missing++;
    }
    
    if (missing == 0) return false;
    
    if (result) {
        result->type = UFT_PROT_MISSING_SECTORS;
        result->name = "Missing Sectors";
        result->family = "Sector Count";
        result->confidence = 80;
        result->offset = 0;
        snprintf(result->notes, sizeof(result->notes),
                 "%d of %d sectors missing", missing, expected_sectors);
    }
    return true;
}

bool uft_prot_detect_bad_crc(const uint8_t *sector_data,
                             size_t sector_len,
                             uint16_t stored_crc,
                             uint16_t computed_crc,
                             uft_protection_result_t *result) {
    (void)sector_data;
    (void)sector_len;
    
    if (stored_crc == computed_crc) return false;
    
    if (result) {
        result->type = UFT_PROT_BAD_CRC;
        result->name = "Intentional Bad CRC";
        result->family = "CRC Protection";
        result->confidence = 70;
        result->offset = 0;
        snprintf(result->notes, sizeof(result->notes),
                 "CRC mismatch: stored 0x%04X, computed 0x%04X",
                 stored_crc, computed_crc);
    }
    return true;
}

/*============================================================================
 * Context Management
 *============================================================================*/

int uft_protection_ctx_init(uft_protection_ctx_t *ctx) {
    if (!ctx) return -1;
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->result_capacity = 32;
    ctx->results = calloc(ctx->result_capacity, sizeof(uft_protection_result_t));
    if (!ctx->results) return -1;
    
    ctx->detect_c64 = true;
    ctx->detect_amiga = true;
    ctx->detect_pc = true;
    
    return 0;
}

void uft_protection_ctx_free(uft_protection_ctx_t *ctx) {
    if (!ctx) return;
    free(ctx->results);
    memset(ctx, 0, sizeof(*ctx));
}

int uft_protection_ctx_add_result(uft_protection_ctx_t *ctx,
                                  const uft_protection_result_t *result) {
    if (!ctx || !result) return -1;
    
    /* Grow if needed */
    if (ctx->result_count >= ctx->result_capacity) {
        size_t new_cap = ctx->result_capacity * 2;
        uft_protection_result_t *new_results = realloc(ctx->results,
            new_cap * sizeof(uft_protection_result_t));
        if (!new_results) return -1;
        ctx->results = new_results;
        ctx->result_capacity = new_cap;
    }
    
    ctx->results[ctx->result_count++] = *result;
    return 0;
}

int uft_protection_scan_disk(uft_protection_ctx_t *ctx,
                             const uint8_t **tracks,
                             const size_t *track_lens,
                             int track_count,
                             int side_count) {
    if (!ctx || !tracks || !track_lens) return -1;
    
    int found = 0;
    
    for (int side = 0; side < side_count; side++) {
        for (int track = 0; track < track_count; track++) {
            int idx = side * track_count + track;
            const uint8_t *data = tracks[idx];
            size_t len = track_lens[idx];
            
            if (!data || len == 0) continue;
            ctx->tracks_scanned++;
            
            uft_protection_result_t result;
            memset(&result, 0, sizeof(result));
            result.track = track;
            result.side = side;
            
            /* C64 protections */
            if (ctx->detect_c64) {
                if (uft_prot_detect_vmax(data, len, &result)) {
                    uft_protection_ctx_add_result(ctx, &result);
                    found++;
                }
                if (uft_prot_detect_vmax_cw(data, len, &result)) {
                    uft_protection_ctx_add_result(ctx, &result);
                    found++;
                }
                if (uft_prot_detect_pirateslayer(data, len, &result)) {
                    uft_protection_ctx_add_result(ctx, &result);
                    found++;
                }
                if (uft_prot_detect_rapidlok(data, len, &result)) {
                    uft_protection_ctx_add_result(ctx, &result);
                    found++;
                }
            }
            
            /* Amiga protections */
            if (ctx->detect_amiga) {
                if (uft_prot_detect_copylock(data, len, &result)) {
                    uft_protection_ctx_add_result(ctx, &result);
                    found++;
                }
                if (uft_prot_detect_long_track(len * 8, &result)) {
                    result.track = track;
                    result.side = side;
                    uft_protection_ctx_add_result(ctx, &result);
                    found++;
                }
                if (uft_prot_detect_rnc_hidden(data, len, &result)) {
                    uft_protection_ctx_add_result(ctx, &result);
                    found++;
                }
            }
        }
    }
    
    ctx->protections_found = found;
    return found;
}

/*============================================================================
 * String Functions
 *============================================================================*/

const char *uft_protection_type_name(uft_protection_type_t type) {
    switch (type) {
        case UFT_PROT_NONE: return "None";
        case UFT_PROT_VMAX: return "V-MAX";
        case UFT_PROT_VMAX_CW: return "V-MAX Cinemaware";
        case UFT_PROT_PIRATESLAYER: return "PirateSlayer";
        case UFT_PROT_PIRATESLAYER_V2: return "PirateSlayer v2";
        case UFT_PROT_RAPIDLOK: return "RapidLok";
        case UFT_PROT_RAPIDLOK_V2: return "RapidLok v2";
        case UFT_PROT_FAT_TRACK: return "Fat Track";
        case UFT_PROT_CUSTOM_GCR: return "Custom GCR";
        case UFT_PROT_COPYLOCK: return "CopyLock";
        case UFT_PROT_COPYLOCK_OLD: return "CopyLock (Old)";
        case UFT_PROT_RNC_PDOS: return "RNC PDOS";
        case UFT_PROT_RNC_PDOS_OLD: return "RNC PDOS (Old)";
        case UFT_PROT_RNC_GAP: return "RNC Gap";
        case UFT_PROT_RNC_HIDDEN: return "RNC Hidden Sectors";
        case UFT_PROT_SPEEDLOCK: return "Speedlock";
        case UFT_PROT_PSYGNOSIS_A: return "Psygnosis A";
        case UFT_PROT_PSYGNOSIS_B: return "Psygnosis B";
        case UFT_PROT_PSYGNOSIS_C: return "Psygnosis C";
        case UFT_PROT_LONG_TRACK: return "Long Track";
        case UFT_PROT_WEAK_BITS: return "Weak Bits";
        case UFT_PROT_FUZZY_BITS: return "Fuzzy Bits";
        case UFT_PROT_EXTRA_SECTORS: return "Extra Sectors";
        case UFT_PROT_MISSING_SECTORS: return "Missing Sectors";
        case UFT_PROT_BAD_CRC: return "Intentional Bad CRC";
        default: return "Unknown";
    }
}

const char *uft_protection_family_name(uft_protection_type_t type) {
    switch (type) {
        case UFT_PROT_VMAX:
        case UFT_PROT_VMAX_CW:
            return "Vorpal";
            
        case UFT_PROT_PIRATESLAYER:
        case UFT_PROT_PIRATESLAYER_V2:
            return "EA/Activision";
            
        case UFT_PROT_RAPIDLOK:
        case UFT_PROT_RAPIDLOK_V2:
            return "Rapidlok";
            
        case UFT_PROT_COPYLOCK:
        case UFT_PROT_COPYLOCK_OLD:
        case UFT_PROT_RNC_PDOS:
        case UFT_PROT_RNC_PDOS_OLD:
        case UFT_PROT_RNC_GAP:
        case UFT_PROT_RNC_HIDDEN:
            return "Rob Northen";
            
        case UFT_PROT_SPEEDLOCK:
            return "Speedlock";
            
        case UFT_PROT_PSYGNOSIS_A:
        case UFT_PROT_PSYGNOSIS_B:
        case UFT_PROT_PSYGNOSIS_C:
            return "Psygnosis";
            
        default:
            return "Generic";
    }
}

int uft_protection_generate_report(const uft_protection_ctx_t *ctx,
                                   char *buffer,
                                   size_t buf_size) {
    if (!ctx || !buffer || buf_size == 0) return -1;
    
    int pos = 0;
    
    pos += snprintf(buffer + pos, buf_size - pos,
        "=== Copy Protection Analysis Report ===\n\n"
        "Tracks scanned: %d\n"
        "Protections found: %d\n\n",
        ctx->tracks_scanned, ctx->protections_found);
    
    if (ctx->result_count > 0) {
        pos += snprintf(buffer + pos, buf_size - pos, "Detections:\n");
        
        for (size_t i = 0; i < ctx->result_count && pos < (int)(buf_size - 256); i++) {
            const uft_protection_result_t *r = &ctx->results[i];
            pos += snprintf(buffer + pos, buf_size - pos,
                "  [%zu] Track %d.%d: %s (%s) - %d%% confidence\n"
                "       %s\n",
                i + 1, r->track, r->side, r->name, r->family,
                r->confidence, r->notes);
        }
    } else {
        pos += snprintf(buffer + pos, buf_size - pos,
            "No copy protection detected.\n");
    }
    
    return pos;
}

/*============================================================================
 * Unit Tests
 *============================================================================*/

#ifdef UFT_UNIT_TESTS

#include <assert.h>

static void test_vmax_detection(void) {
    /* Create test data with V-MAX marker */
    uint8_t track[100];
    memset(track, 0x00, sizeof(track));
    memcpy(track + 20, UFT_VMAX_MARKERS, sizeof(UFT_VMAX_MARKERS));
    
    uft_protection_result_t result;
    const uint8_t *pos = uft_prot_detect_vmax(track, sizeof(track), &result);
    
    assert(pos != NULL);
    assert(pos == track + 20);
    assert(result.type == UFT_PROT_VMAX);
    assert(result.confidence >= 90);
    
    printf("  ✓ vmax_detection passed\n");
}

static void test_rapidlok_detection(void) {
    /* Create test data with RapidLok header */
    uint8_t track[300];
    memset(track, 0x00, sizeof(track));
    
    /* 25 sync bytes */
    memset(track + 10, 0xFF, 25);
    /* ID byte */
    track[35] = 0x55;
    /* 170 header bytes */
    memset(track + 36, 0x7B, 170);
    
    uft_protection_result_t result;
    const uint8_t *pos = uft_prot_detect_rapidlok(track, sizeof(track), &result);
    
    assert(pos != NULL);
    assert(result.type == UFT_PROT_RAPIDLOK);
    assert(result.confidence >= 90);
    
    printf("  ✓ rapidlok_detection passed\n");
}

static void test_weak_bits_detection(void) {
    /* Create test data with inconsistent bytes */
    uint8_t read1[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint8_t read2[] = {0x01, 0x02, 0xFF, 0x04, 0x05};  /* byte 2 differs */
    uint8_t read3[] = {0x01, 0x02, 0xAA, 0x04, 0x05};  /* byte 2 differs */
    
    const uint8_t *reads[] = {read1, read2, read3};
    uint8_t weak_map[5];
    size_t weak_count;
    
    bool found = uft_prot_detect_weak_bits(reads, 3, 5, weak_map, &weak_count);
    
    assert(found);
    assert(weak_count == 1);
    assert(weak_map[2] == 1);
    
    printf("  ✓ weak_bits_detection passed\n");
}

static void test_context_management(void) {
    uft_protection_ctx_t ctx;
    assert(uft_protection_ctx_init(&ctx) == 0);
    
    uft_protection_result_t result = {
        .type = UFT_PROT_VMAX,
        .name = "Test",
        .confidence = 90
    };
    
    assert(uft_protection_ctx_add_result(&ctx, &result) == 0);
    assert(ctx.result_count == 1);
    
    uft_protection_ctx_free(&ctx);
    
    printf("  ✓ context_management passed\n");
}

static void test_string_functions(void) {
    assert(strcmp(uft_protection_type_name(UFT_PROT_VMAX), "V-MAX") == 0);
    assert(strcmp(uft_protection_family_name(UFT_PROT_COPYLOCK), "Rob Northen") == 0);
    
    printf("  ✓ string_functions passed\n");
}

void uft_protection_detect_tests(void) {
    printf("Running Protection Detection tests...\n");
    test_vmax_detection();
    test_rapidlok_detection();
    test_weak_bits_detection();
    test_context_management();
    test_string_functions();
    printf("All Protection Detection tests passed!\n");
}

#endif /* UFT_UNIT_TESTS */
