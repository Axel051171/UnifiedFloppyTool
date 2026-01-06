/**
 * @file uft_longtrack.c
 * @brief Longtrack Protection Collection Implementation
 * 
 * Clean-room reimplementation based on algorithm analysis.
 * 
 * SPDX-License-Identifier: MIT
 */

#include "uft/protection/uft_longtrack.h"
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

/**
 * @brief Extract byte from bitstream at bit position
 */
static uint8_t get_byte_at_bit(const uint8_t *data, uint32_t bit_pos) {
    uint32_t byte_pos = bit_pos / 8;
    uint32_t bit_off = bit_pos % 8;
    
    if (bit_off == 0) {
        return data[byte_pos];
    }
    
    return (data[byte_pos] << bit_off) | (data[byte_pos + 1] >> (8 - bit_off));
}

/**
 * @brief Extract 16-bit word from bitstream
 */
static uint16_t get_word_at_bit(const uint8_t *data, uint32_t bit_pos) {
    return ((uint16_t)get_byte_at_bit(data, bit_pos) << 8) |
           get_byte_at_bit(data, bit_pos + 8);
}

/**
 * @brief Extract 32-bit word from bitstream
 */
static uint32_t get_dword_at_bit(const uint8_t *data, uint32_t bit_pos) {
    return ((uint32_t)get_word_at_bit(data, bit_pos) << 16) |
           get_word_at_bit(data, bit_pos + 16);
}

/**
 * @brief Calculate byte histogram
 */
static void calc_histogram(const uint8_t *data, uint32_t bytes,
                           uint8_t *histogram, uint8_t *dominant) {
    memset(histogram, 0, 256);
    
    for (uint32_t i = 0; i < bytes; i++) {
        histogram[data[i]]++;
    }
    
    /* Find dominant byte */
    uint8_t max_count = 0;
    *dominant = 0;
    for (int i = 0; i < 256; i++) {
        if (histogram[i] > max_count) {
            max_count = histogram[i];
            *dominant = (uint8_t)i;
        }
    }
}

/*===========================================================================
 * Sync Detection
 *===========================================================================*/

int32_t uft_longtrack_find_sync(const uint8_t *track_data,
                                 uint32_t track_bits,
                                 uint32_t sync,
                                 bool is_32bit) {
    if (!track_data || track_bits < 32) return -1;
    
    uint32_t end_bit = track_bits - (is_32bit ? 32 : 16);
    
    for (uint32_t bit = 0; bit <= end_bit; bit += 8) {
        if (is_32bit) {
            if (get_dword_at_bit(track_data, bit) == sync) {
                return (int32_t)bit;
            }
        } else {
            if (get_word_at_bit(track_data, bit) == (uint16_t)sync) {
                return (int32_t)bit;
            }
        }
    }
    
    return -1;
}

/*===========================================================================
 * Pattern Analysis
 *===========================================================================*/

uint32_t uft_longtrack_analyze_pattern(const uint8_t *track_data,
                                        uint32_t track_bits,
                                        uint32_t start_bit,
                                        uint8_t *pattern,
                                        float *match) {
    if (!track_data || !pattern || !match) return 0;
    
    uint32_t start_byte = start_bit / 8;
    uint32_t total_bytes = track_bits / 8;
    
    if (start_byte >= total_bytes) return 0;
    
    uint32_t analyze_bytes = total_bytes - start_byte;
    if (analyze_bytes > 1000) analyze_bytes = 1000; /* Limit analysis */
    
    /* Find dominant pattern byte */
    uint8_t histogram[256];
    uint8_t dominant;
    calc_histogram(&track_data[start_byte], analyze_bytes, histogram, &dominant);
    
    *pattern = dominant;
    
    /* Calculate match percentage */
    uint32_t matching = 0;
    for (uint32_t i = 0; i < analyze_bytes; i++) {
        if (track_data[start_byte + i] == dominant) {
            matching++;
        }
    }
    
    *match = (float)matching / analyze_bytes * 100.0f;
    
    /* Find continuous pattern region length */
    uint32_t pattern_len = 0;
    for (uint32_t i = 0; i < analyze_bytes; i++) {
        if (track_data[start_byte + i] == dominant) {
            pattern_len++;
        } else if (pattern_len > 100) {
            break; /* Found significant region */
        } else {
            pattern_len = 0;
        }
    }
    
    return pattern_len * 8; /* Convert to bits */
}

/*===========================================================================
 * Type-Specific Detectors
 *===========================================================================*/

bool uft_longtrack_detect_protec(const uint8_t *track_data,
                                  uint32_t track_bits,
                                  uft_longtrack_info_t *info) {
    if (!track_data || !info) return false;
    
    const uft_longtrack_def_t *def = &uft_longtrack_defs[0]; /* PROTEC */
    
    if (track_bits < def->min_bits) return false;
    
    /* Find sync 0x4454 */
    int32_t sync_pos = uft_longtrack_find_sync(track_data, track_bits,
                                                def->sync_word, false);
    
    if (sync_pos < 0) return false;
    
    /* Analyze pattern after sync */
    uint8_t pattern;
    float match;
    uint32_t pattern_len = uft_longtrack_analyze_pattern(
        track_data, track_bits, (uint32_t)sync_pos + 16, &pattern, &match);
    
    /* PROTEC typically uses 0x33 pattern, but can vary */
    info->type = UFT_LONGTRACK_PROTEC;
    info->def = def;
    info->sync_word = def->sync_word;
    info->sync_offset = sync_pos;
    info->min_track_bits = def->min_bits;
    info->actual_track_bits = track_bits;
    info->length_ratio = (float)track_bits / UFT_LONGTRACK_AMIGA_NORMAL;
    info->pattern_byte = pattern;
    info->pattern_start = (uint32_t)sync_pos + 16;
    info->pattern_length = pattern_len;
    info->pattern_match = match;
    
    return true;
}

bool uft_longtrack_detect_protoscan(const uint8_t *track_data,
                                     uint32_t track_bits,
                                     uft_longtrack_info_t *info) {
    if (!track_data || !info) return false;
    
    const uft_longtrack_def_t *def = &uft_longtrack_defs[1]; /* Protoscan */
    
    if (track_bits < def->min_bits) return false;
    
    /* Find 32-bit sync 0x41244124 */
    int32_t sync_pos = uft_longtrack_find_sync(track_data, track_bits,
                                                def->sync_word, true);
    
    if (sync_pos < 0) return false;
    
    /* Protoscan uses 0x00 fill */
    uint8_t pattern;
    float match;
    uint32_t pattern_len = uft_longtrack_analyze_pattern(
        track_data, track_bits, (uint32_t)sync_pos + 32, &pattern, &match);
    
    if (pattern != 0x00 || match < 70.0f) return false;
    
    info->type = UFT_LONGTRACK_PROTOSCAN;
    info->def = def;
    info->sync_word = def->sync_word;
    info->sync_offset = sync_pos;
    info->min_track_bits = def->min_bits;
    info->actual_track_bits = track_bits;
    info->length_ratio = (float)track_bits / UFT_LONGTRACK_AMIGA_NORMAL;
    info->pattern_byte = pattern;
    info->pattern_start = (uint32_t)sync_pos + 32;
    info->pattern_length = pattern_len;
    info->pattern_match = match;
    
    return true;
}

bool uft_longtrack_detect_tiertex(const uint8_t *track_data,
                                   uint32_t track_bits,
                                   uft_longtrack_info_t *info) {
    if (!track_data || !info) return false;
    
    const uft_longtrack_def_t *def = &uft_longtrack_defs[2]; /* Tiertex */
    
    /* Tiertex has specific length range */
    if (track_bits < def->min_bits || track_bits > def->max_bits) return false;
    
    /* Same sync as Protoscan */
    int32_t sync_pos = uft_longtrack_find_sync(track_data, track_bits,
                                                def->sync_word, true);
    
    if (sync_pos < 0) return false;
    
    uint8_t pattern;
    float match;
    uint32_t pattern_len = uft_longtrack_analyze_pattern(
        track_data, track_bits, (uint32_t)sync_pos + 32, &pattern, &match);
    
    info->type = UFT_LONGTRACK_TIERTEX;
    info->def = def;
    info->sync_word = def->sync_word;
    info->sync_offset = sync_pos;
    info->min_track_bits = def->min_bits;
    info->actual_track_bits = track_bits;
    info->length_ratio = (float)track_bits / UFT_LONGTRACK_AMIGA_NORMAL;
    info->pattern_byte = pattern;
    info->pattern_start = (uint32_t)sync_pos + 32;
    info->pattern_length = pattern_len;
    info->pattern_match = match;
    
    return true;
}

bool uft_longtrack_detect_silmarils(const uint8_t *track_data,
                                     uint32_t track_bits,
                                     uft_longtrack_info_t *info) {
    if (!track_data || !info) return false;
    
    const uft_longtrack_def_t *def = &uft_longtrack_defs[3]; /* Silmarils */
    
    if (track_bits < def->min_bits) return false;
    
    /* Find sync 0xA144 */
    int32_t sync_pos = uft_longtrack_find_sync(track_data, track_bits,
                                                def->sync_word, false);
    
    if (sync_pos < 0) return false;
    
    /* Look for "ROD0" signature */
    bool sig_found = false;
    uint32_t sig_search_start = (uint32_t)sync_pos / 8;
    uint32_t sig_search_end = sig_search_start + 256;
    if (sig_search_end > track_bits / 8 - 4) {
        sig_search_end = track_bits / 8 - 4;
    }
    
    for (uint32_t i = sig_search_start; i < sig_search_end; i++) {
        if (memcmp(&track_data[i], "ROD0", 4) == 0) {
            sig_found = true;
            memcpy(info->signature, "ROD0", 4);
            info->signature_len = 4;
            info->signature_found = true;
            break;
        }
    }
    
    uint8_t pattern;
    float match;
    uint32_t pattern_len = uft_longtrack_analyze_pattern(
        track_data, track_bits, (uint32_t)sync_pos + 16, &pattern, &match);
    
    info->type = UFT_LONGTRACK_SILMARILS;
    info->def = def;
    info->sync_word = def->sync_word;
    info->sync_offset = sync_pos;
    info->min_track_bits = def->min_bits;
    info->actual_track_bits = track_bits;
    info->length_ratio = (float)track_bits / UFT_LONGTRACK_AMIGA_NORMAL;
    info->pattern_byte = pattern;
    info->pattern_start = (uint32_t)sync_pos + 16;
    info->pattern_length = pattern_len;
    info->pattern_match = match;
    
    return sig_found; /* Require signature for Silmarils */
}

bool uft_longtrack_detect_infogrames(const uint8_t *track_data,
                                      uint32_t track_bits,
                                      uft_longtrack_info_t *info) {
    if (!track_data || !info) return false;
    
    const uft_longtrack_def_t *def = &uft_longtrack_defs[4]; /* Infogrames */
    
    if (track_bits < def->min_bits) return false;
    
    /* Same sync as Silmarils but no signature */
    int32_t sync_pos = uft_longtrack_find_sync(track_data, track_bits,
                                                def->sync_word, false);
    
    if (sync_pos < 0) return false;
    
    /* Check it's NOT Silmarils (no ROD0 signature) */
    bool is_silmarils = false;
    uint32_t sig_search_start = (uint32_t)sync_pos / 8;
    uint32_t sig_search_end = sig_search_start + 256;
    if (sig_search_end > track_bits / 8 - 4) {
        sig_search_end = track_bits / 8 - 4;
    }
    
    for (uint32_t i = sig_search_start; i < sig_search_end; i++) {
        if (memcmp(&track_data[i], "ROD0", 4) == 0) {
            is_silmarils = true;
            break;
        }
    }
    
    if (is_silmarils) return false;
    
    uint8_t pattern;
    float match;
    uint32_t pattern_len = uft_longtrack_analyze_pattern(
        track_data, track_bits, (uint32_t)sync_pos + 16, &pattern, &match);
    
    info->type = UFT_LONGTRACK_INFOGRAMES;
    info->def = def;
    info->sync_word = def->sync_word;
    info->sync_offset = sync_pos;
    info->min_track_bits = def->min_bits;
    info->actual_track_bits = track_bits;
    info->length_ratio = (float)track_bits / UFT_LONGTRACK_AMIGA_NORMAL;
    info->pattern_byte = pattern;
    info->pattern_start = (uint32_t)sync_pos + 16;
    info->pattern_length = pattern_len;
    info->pattern_match = match;
    
    return true;
}

bool uft_longtrack_detect_prolance(const uint8_t *track_data,
                                    uint32_t track_bits,
                                    uft_longtrack_info_t *info) {
    if (!track_data || !info) return false;
    
    const uft_longtrack_def_t *def = &uft_longtrack_defs[5]; /* Prolance */
    
    if (track_bits < def->min_bits) return false;
    
    /* Find sync 0x8945 */
    int32_t sync_pos = uft_longtrack_find_sync(track_data, track_bits,
                                                def->sync_word, false);
    
    if (sync_pos < 0) return false;
    
    uint8_t pattern;
    float match;
    uint32_t pattern_len = uft_longtrack_analyze_pattern(
        track_data, track_bits, (uint32_t)sync_pos + 16, &pattern, &match);
    
    info->type = UFT_LONGTRACK_PROLANCE;
    info->def = def;
    info->sync_word = def->sync_word;
    info->sync_offset = sync_pos;
    info->min_track_bits = def->min_bits;
    info->actual_track_bits = track_bits;
    info->length_ratio = (float)track_bits / UFT_LONGTRACK_AMIGA_NORMAL;
    info->pattern_byte = pattern;
    info->pattern_start = (uint32_t)sync_pos + 16;
    info->pattern_length = pattern_len;
    info->pattern_match = match;
    
    return true;
}

bool uft_longtrack_detect_app(const uint8_t *track_data,
                               uint32_t track_bits,
                               uft_longtrack_info_t *info) {
    if (!track_data || !info) return false;
    
    const uft_longtrack_def_t *def = &uft_longtrack_defs[6]; /* APP */
    
    if (track_bits < def->min_bits) return false;
    
    /* Find sync 0x924A */
    int32_t sync_pos = uft_longtrack_find_sync(track_data, track_bits,
                                                def->sync_word, false);
    
    if (sync_pos < 0) return false;
    
    uint8_t pattern;
    float match;
    uint32_t pattern_len = uft_longtrack_analyze_pattern(
        track_data, track_bits, (uint32_t)sync_pos + 16, &pattern, &match);
    
    /* APP uses 0xDC pattern */
    if (pattern != 0xDC && match < 50.0f) return false;
    
    info->type = UFT_LONGTRACK_APP;
    info->def = def;
    info->sync_word = def->sync_word;
    info->sync_offset = sync_pos;
    info->min_track_bits = def->min_bits;
    info->actual_track_bits = track_bits;
    info->length_ratio = (float)track_bits / UFT_LONGTRACK_AMIGA_NORMAL;
    info->pattern_byte = pattern;
    info->pattern_start = (uint32_t)sync_pos + 16;
    info->pattern_length = pattern_len;
    info->pattern_match = match;
    
    return true;
}

bool uft_longtrack_detect_sevencities(const uint8_t *track_data,
                                       uint32_t track_bits,
                                       uft_longtrack_info_t *info) {
    if (!track_data || !info) return false;
    
    const uft_longtrack_def_t *def = &uft_longtrack_defs[7]; /* SevenCities */
    
    if (track_bits < def->min_bits) return false;
    
    /* Try both sync words */
    int32_t sync_pos = uft_longtrack_find_sync(track_data, track_bits,
                                                def->sync_word, false);
    uint32_t found_sync = def->sync_word;
    
    if (sync_pos < 0 && def->sync_word_alt != 0) {
        sync_pos = uft_longtrack_find_sync(track_data, track_bits,
                                            def->sync_word_alt, false);
        found_sync = def->sync_word_alt;
    }
    
    if (sync_pos < 0) return false;
    
    uint8_t pattern;
    float match;
    uint32_t pattern_len = uft_longtrack_analyze_pattern(
        track_data, track_bits, (uint32_t)sync_pos + 16, &pattern, &match);
    
    info->type = UFT_LONGTRACK_SEVENCITIES;
    info->def = def;
    info->sync_word = found_sync;
    info->sync_offset = sync_pos;
    info->min_track_bits = def->min_bits;
    info->actual_track_bits = track_bits;
    info->length_ratio = (float)track_bits / UFT_LONGTRACK_AMIGA_NORMAL;
    info->pattern_byte = pattern;
    info->pattern_start = (uint32_t)sync_pos + 16;
    info->pattern_length = pattern_len;
    info->pattern_match = match;
    
    return true;
}

bool uft_longtrack_detect_supermethanebros(const uint8_t *track_data,
                                            uint32_t track_bits,
                                            uft_longtrack_info_t *info) {
    if (!track_data || !info) return false;
    
    const uft_longtrack_def_t *def = &uft_longtrack_defs[8]; /* SuperMethaneBros */
    
    /* GCR tracks are half the bit count */
    if (track_bits < def->min_bits) return false;
    
    /* Find GCR sync 0x99999999 */
    int32_t sync_pos = uft_longtrack_find_sync(track_data, track_bits,
                                                def->sync_word, true);
    
    if (sync_pos < 0) return false;
    
    info->type = UFT_LONGTRACK_SUPERMETHANEBROS;
    info->def = def;
    info->sync_word = def->sync_word;
    info->sync_offset = sync_pos;
    info->min_track_bits = def->min_bits;
    info->actual_track_bits = track_bits;
    info->length_ratio = (float)track_bits / (UFT_LONGTRACK_AMIGA_NORMAL / 2);
    info->pattern_byte = 0xFF;
    info->pattern_start = (uint32_t)sync_pos + 32;
    info->pattern_length = 0;
    info->pattern_match = 0;
    
    return true;
}

/*===========================================================================
 * Main Detection
 *===========================================================================*/

bool uft_longtrack_detect_type(const uint8_t *track_data,
                                uint32_t track_bits,
                                uft_longtrack_type_t type,
                                uft_longtrack_info_t *info) {
    switch (type) {
        case UFT_LONGTRACK_PROTEC:
            return uft_longtrack_detect_protec(track_data, track_bits, info);
        case UFT_LONGTRACK_PROTOSCAN:
            return uft_longtrack_detect_protoscan(track_data, track_bits, info);
        case UFT_LONGTRACK_TIERTEX:
            return uft_longtrack_detect_tiertex(track_data, track_bits, info);
        case UFT_LONGTRACK_SILMARILS:
            return uft_longtrack_detect_silmarils(track_data, track_bits, info);
        case UFT_LONGTRACK_INFOGRAMES:
            return uft_longtrack_detect_infogrames(track_data, track_bits, info);
        case UFT_LONGTRACK_PROLANCE:
            return uft_longtrack_detect_prolance(track_data, track_bits, info);
        case UFT_LONGTRACK_APP:
            return uft_longtrack_detect_app(track_data, track_bits, info);
        case UFT_LONGTRACK_SEVENCITIES:
            return uft_longtrack_detect_sevencities(track_data, track_bits, info);
        case UFT_LONGTRACK_SUPERMETHANEBROS:
            return uft_longtrack_detect_supermethanebros(track_data, track_bits, info);
        default:
            return false;
    }
}

int uft_longtrack_detect(const uint8_t *track_data,
                         uint32_t track_bits,
                         uint8_t track,
                         uint8_t head,
                         uft_longtrack_result_t *result) {
    if (!result) return -1;
    
    memset(result, 0, sizeof(*result));
    result->track = track;
    result->head = head;
    result->track_bits = track_bits;
    
    if (!track_data || track_bits < UFT_LONGTRACK_AMIGA_NORMAL) {
        snprintf(result->info, sizeof(result->info),
                 "Track too short for longtrack analysis");
        return 0;
    }
    
    /* Calculate basic statistics */
    uint32_t track_bytes = track_bits / 8;
    calc_histogram(track_data, track_bytes, result->byte_histogram,
                   &result->dominant_byte);
    
    /* Calculate homogeneity */
    uint32_t dominant_count = 0;
    for (uint32_t i = 0; i < track_bytes; i++) {
        if (track_data[i] == result->dominant_byte) {
            dominant_count++;
        }
    }
    result->homogeneity = (float)dominant_count / track_bytes * 100.0f;
    
    /* Check if track is long enough */
    if (!uft_longtrack_is_long(track_bits)) {
        snprintf(result->info, sizeof(result->info),
                 "Track is normal length (%u bits)", track_bits);
        return 0;
    }
    
    /* Try each detector in priority order */
    typedef bool (*detector_fn)(const uint8_t*, uint32_t, uft_longtrack_info_t*);
    
    static const detector_fn detectors[] = {
        uft_longtrack_detect_protec,
        uft_longtrack_detect_silmarils,    /* Before Infogrames (has signature) */
        uft_longtrack_detect_infogrames,
        uft_longtrack_detect_app,
        uft_longtrack_detect_prolance,
        uft_longtrack_detect_tiertex,      /* Before Protoscan (specific range) */
        uft_longtrack_detect_protoscan,
        uft_longtrack_detect_sevencities,
        uft_longtrack_detect_supermethanebros
    };
    
    const int num_detectors = sizeof(detectors) / sizeof(detectors[0]);
    
    for (int i = 0; i < num_detectors; i++) {
        uft_longtrack_info_t info;
        memset(&info, 0, sizeof(info));
        
        if (detectors[i](track_data, track_bits, &info)) {
            if (!result->detected) {
                /* First match becomes primary */
                result->detected = true;
                result->primary = info;
                
                /* Determine confidence */
                if (info.signature_found) {
                    result->confidence = UFT_LONGTRACK_CONF_CERTAIN;
                } else if (info.pattern_match > 80.0f) {
                    result->confidence = UFT_LONGTRACK_CONF_CERTAIN;
                } else if (info.sync_offset >= 0) {
                    result->confidence = UFT_LONGTRACK_CONF_LIKELY;
                } else {
                    result->confidence = UFT_LONGTRACK_CONF_POSSIBLE;
                }
            } else if (result->candidate_count < 3) {
                /* Additional matches become candidates */
                result->candidates[result->candidate_count++] = info;
            }
        }
    }
    
    /* If nothing detected but track is long, mark as generic */
    if (!result->detected && uft_longtrack_is_long(track_bits)) {
        result->detected = true;
        result->confidence = UFT_LONGTRACK_CONF_POSSIBLE;
        
        /* Check for empty or zeroes */
        if (result->dominant_byte == 0xFF && result->homogeneity > 90.0f) {
            result->primary.type = UFT_LONGTRACK_EMPTY;
            result->primary.def = &uft_longtrack_defs[9];
        } else if (result->dominant_byte == 0x00 && result->homogeneity > 90.0f) {
            result->primary.type = UFT_LONGTRACK_ZEROES;
            result->primary.def = &uft_longtrack_defs[10];
        } else {
            result->primary.type = UFT_LONGTRACK_UNKNOWN;
        }
        
        result->primary.actual_track_bits = track_bits;
        result->primary.length_ratio = (float)track_bits / UFT_LONGTRACK_AMIGA_NORMAL;
        result->primary.pattern_byte = result->dominant_byte;
        result->primary.pattern_match = result->homogeneity;
    }
    
    /* Generate info string */
    if (result->detected) {
        snprintf(result->info, sizeof(result->info),
                 "%s longtrack: %u bits (%.1f%%), sync=0x%X, pattern=0x%02X (%.1f%%)",
                 uft_longtrack_type_name(result->primary.type),
                 track_bits,
                 result->primary.length_ratio * 100.0f,
                 result->primary.sync_word,
                 result->primary.pattern_byte,
                 result->primary.pattern_match);
    } else {
        snprintf(result->info, sizeof(result->info),
                 "Not a longtrack (%u bits)", track_bits);
    }
    
    return 0;
}

/*===========================================================================
 * Reporting
 *===========================================================================*/

const char* uft_longtrack_type_name(uft_longtrack_type_t type) {
    if (type < UFT_LONGTRACK_TYPE_COUNT) {
        return uft_longtrack_defs[type].name;
    }
    return "Unknown";
}

const char* uft_longtrack_confidence_name(uft_longtrack_confidence_t conf) {
    switch (conf) {
        case UFT_LONGTRACK_CONF_NONE: return "Not Detected";
        case UFT_LONGTRACK_CONF_POSSIBLE: return "Possible";
        case UFT_LONGTRACK_CONF_LIKELY: return "Likely";
        case UFT_LONGTRACK_CONF_CERTAIN: return "Certain";
        default: return "Unknown";
    }
}

const uft_longtrack_def_t* uft_longtrack_get_def(uft_longtrack_type_t type) {
    if (type < UFT_LONGTRACK_TYPE_COUNT) {
        return &uft_longtrack_defs[type];
    }
    return NULL;
}

size_t uft_longtrack_report(const uft_longtrack_result_t *result,
                             char *buffer, size_t buffer_size) {
    if (!result || !buffer || buffer_size == 0) return 0;
    
    size_t w = 0;
    
    w += snprintf(buffer + w, buffer_size - w,
        "=== Longtrack Analysis Report ===\n\n"
        "Detection: %s\n"
        "Confidence: %s\n\n"
        "Track: %d, Head: %d\n"
        "Track bits: %u (%.1f%% of normal)\n\n",
        result->detected ? "YES" : "NO",
        uft_longtrack_confidence_name(result->confidence),
        result->track, result->head,
        result->track_bits,
        (float)result->track_bits / UFT_LONGTRACK_AMIGA_NORMAL * 100.0f);
    
    if (w >= buffer_size) return buffer_size - 1;
    
    if (result->detected) {
        const uft_longtrack_info_t *p = &result->primary;
        
        w += snprintf(buffer + w, buffer_size - w,
            "Primary Detection:\n"
            "  Type: %s\n"
            "  Sync Word: 0x%X @ bit %d\n"
            "  Pattern Byte: 0x%02X (%.1f%% match)\n"
            "  Pattern Length: %u bits\n",
            uft_longtrack_type_name(p->type),
            p->sync_word,
            p->sync_offset,
            p->pattern_byte,
            p->pattern_match,
            p->pattern_length);
        
        if (p->signature_found) {
            w += snprintf(buffer + w, buffer_size - w,
                "  Signature: \"%.16s\"\n", p->signature);
        }
        
        if (p->def) {
            w += snprintf(buffer + w, buffer_size - w,
                "\nExpected Parameters:\n"
                "  Min bits: %u\n"
                "  Max bits: %u\n"
                "  Expected pattern: 0x%02X\n"
                "  GCR encoded: %s\n",
                p->def->min_bits,
                p->def->max_bits,
                p->def->pattern_byte,
                p->def->is_gcr ? "YES" : "NO");
        }
        
        if (result->candidate_count > 0) {
            w += snprintf(buffer + w, buffer_size - w,
                "\nAlternative Candidates (%d):\n", result->candidate_count);
            
            for (uint8_t i = 0; i < result->candidate_count && w < buffer_size; i++) {
                w += snprintf(buffer + w, buffer_size - w,
                    "  [%d] %s (sync 0x%X)\n",
                    i + 1,
                    uft_longtrack_type_name(result->candidates[i].type),
                    result->candidates[i].sync_word);
            }
        }
    }
    
    if (w < buffer_size) {
        w += snprintf(buffer + w, buffer_size - w,
            "\nTrack Statistics:\n"
            "  Dominant byte: 0x%02X\n"
            "  Homogeneity: %.1f%%\n",
            result->dominant_byte,
            result->homogeneity);
    }
    
    return w;
}

size_t uft_longtrack_export_json(const uft_longtrack_result_t *result,
                                  char *buffer, size_t buffer_size) {
    if (!result || !buffer || buffer_size == 0) return 0;
    
    size_t w = 0;
    
    w += snprintf(buffer + w, buffer_size - w,
        "{\n"
        "  \"protection_type\": \"Longtrack\",\n"
        "  \"detected\": %s,\n"
        "  \"confidence\": \"%s\",\n"
        "  \"track\": %d,\n"
        "  \"head\": %d,\n"
        "  \"track_bits\": %u,\n"
        "  \"length_ratio\": %.3f,\n",
        result->detected ? "true" : "false",
        uft_longtrack_confidence_name(result->confidence),
        result->track, result->head,
        result->track_bits,
        (float)result->track_bits / UFT_LONGTRACK_AMIGA_NORMAL);
    
    if (result->detected && w < buffer_size) {
        const uft_longtrack_info_t *p = &result->primary;
        
        w += snprintf(buffer + w, buffer_size - w,
            "  \"primary\": {\n"
            "    \"type\": \"%s\",\n"
            "    \"sync_word\": \"0x%X\",\n"
            "    \"sync_offset\": %d,\n"
            "    \"pattern_byte\": \"0x%02X\",\n"
            "    \"pattern_match\": %.2f,\n"
            "    \"pattern_length\": %u,\n"
            "    \"signature_found\": %s\n"
            "  },\n",
            uft_longtrack_type_name(p->type),
            p->sync_word,
            p->sync_offset,
            p->pattern_byte,
            p->pattern_match,
            p->pattern_length,
            p->signature_found ? "true" : "false");
    }
    
    if (w < buffer_size) {
        w += snprintf(buffer + w, buffer_size - w,
            "  \"statistics\": {\n"
            "    \"dominant_byte\": \"0x%02X\",\n"
            "    \"homogeneity\": %.2f\n"
            "  }\n"
            "}\n",
            result->dominant_byte,
            result->homogeneity);
    }
    
    return w;
}
