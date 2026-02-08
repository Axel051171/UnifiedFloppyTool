/**
 * @file uft_protection_suite.c
 * @brief UFT Copy Protection Suite - Iteration 3 Implementation
 * @version 3.2.0.002
 *
 * Complete implementation of protection detection for:
 * - S-001: C64 Protection Suite (V-MAX!, RapidLok, Vorpal, Fat Tracks, GCR Timing)
 * - S-002: Apple II Protection Suite (Nibble Count, Timing Bits, Spiral Track)
 * - S-003: Atari ST Protection Suite (Macrodos, Copylock ST, Flaschel)
 *
 * "Kein Bit verloren" - Every protection scheme preserved faithfully
 *
 * @copyright UFT Project 2026
 */

#include "uft/uft_protection.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*============================================================================
 * INTERNAL CONSTANTS
 *============================================================================*/

/* C64 GCR Constants */
#define C64_NOMINAL_TRACK_BITS_ZONE0    7692    /* Tracks 1-17 */
#define C64_NOMINAL_TRACK_BITS_ZONE1    7142    /* Tracks 18-24 */
#define C64_NOMINAL_TRACK_BITS_ZONE2    6666    /* Tracks 25-30 */
#define C64_NOMINAL_TRACK_BITS_ZONE3    6250    /* Tracks 31-35 */
#define C64_LONG_TRACK_THRESHOLD        105     /* >105% = long track */
#define C64_SHORT_TRACK_THRESHOLD       95      /* <95% = short track */
#define C64_FAT_TRACK_BITS             8192    /* Fat track size */
#define C64_SYNC_MIN_BITS              10      /* Minimum sync run */
#define C64_SYNC_LONG_BITS             40      /* Long sync run */

/* Apple II Constants */
#define APPLE_TRACK_BYTES              6656    /* Nominal track size */
#define APPLE_NIBBLE_COUNT_TOLERANCE   32      /* Variation threshold */
#define APPLE_SYNC_BYTE                0xFF
#define APPLE_ADDRESS_PROLOGUE_D5      0xD5
#define APPLE_ADDRESS_PROLOGUE_AA      0xAA
#define APPLE_ADDRESS_PROLOGUE_96      0x96
#define APPLE_DATA_PROLOGUE_AD         0xAD

/* Atari ST Constants */
#define ATARI_NOMINAL_TRACK_BYTES      6250    /* Standard track */
#define ATARI_LONG_TRACK_BYTES         6500    /* Long track threshold */
#define ATARI_SHORT_TRACK_BYTES        6000    /* Short track threshold */
#define ATARI_COPYLOCK_TRACK           79      /* Protection track */
#define ATARI_MFM_SYNC_WORD            0x4489  /* Standard MFM sync */

/*============================================================================
 * GCR TABLES (C64)
 *============================================================================*/

static const uint8_t gcr_to_nybble[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF
};

static const uint8_t nybble_to_gcr[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
};

/*============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

static inline int get_bit(const uint8_t* data, size_t offset)
{
    return (data[offset >> 3] >> (7 - (offset & 7))) & 1;
}

static inline void set_bit(uint8_t* data, size_t offset, int value)
{
    if (value) {
        data[offset >> 3] |= (1 << (7 - (offset & 7)));
    } else {
        data[offset >> 3] &= ~(1 << (7 - (offset & 7)));
    }
}

static inline uint16_t get_word_be(const uint8_t* data, size_t bit_offset)
{
    uint16_t word = 0;
    for (int i = 0; i < 16; i++) {
        word = (word << 1) | get_bit(data, bit_offset + i);
    }
    return word;
}

static inline uint32_t get_dword_be(const uint8_t* data, size_t bit_offset)
{
    uint32_t dword = 0;
    for (int i = 0; i < 32; i++) {
        dword = (dword << 1) | get_bit(data, bit_offset + i);
    }
    return dword;
}

/**
 * @brief Decode 5-bit GCR to 4-bit nybble
 */
static inline uint8_t gcr_decode_nybble(uint8_t gcr5)
{
    return gcr_to_nybble[gcr5 & 0x1F];
}

/**
 * @brief Check if GCR byte is valid
 */
static bool is_valid_gcr(uint8_t gcr5)
{
    return gcr_to_nybble[gcr5 & 0x1F] != 0xFF;
}

/**
 * @brief Count consecutive sync bits in C64 bitstream
 */
static uint32_t count_sync_run(const uint8_t* data, size_t bit_count, 
                                size_t start_bit)
{
    uint32_t count = 0;
    while (start_bit + count < bit_count) {
        if (!get_bit(data, start_bit + count)) break;
        count++;
        
        /* Safety limit */
        if (count > 1000) break;
    }
    return count;
}

/**
 * @brief Find pattern in bitstream
 */
static int32_t find_pattern_bits(const uint8_t* data, size_t bit_count,
                                  size_t start_bit, uint32_t pattern, 
                                  uint8_t pattern_bits)
{
    if (pattern_bits > 32) return -1;
    
    uint32_t mask = (1UL << pattern_bits) - 1;
    
    for (size_t i = start_bit; i + pattern_bits <= bit_count; i++) {
        uint32_t value = 0;
        for (uint8_t b = 0; b < pattern_bits; b++) {
            value = (value << 1) | get_bit(data, i + b);
        }
        if ((value & mask) == (pattern & mask)) {
            return (int32_t)i;
        }
    }
    return -1;
}

/*============================================================================
 * C64 PROTECTION DETECTION (S-001)
 *============================================================================*/

/**
 * @brief Detect V-MAX! protection and determine version
 */
uint8_t uft_prot_c64_detect_vmax(
    const uint8_t* gcr_data,
    size_t gcr_bits,
    uint8_t track,
    uft_prot_scheme_t* scheme)
{
    if (!gcr_data || gcr_bits < 1000 || !scheme) {
        return 0;
    }
    
    memset(scheme, 0, sizeof(*scheme));
    uint8_t confidence = 0;
    
    /* V-MAX! characteristics:
     * - Track 20 contains loader
     * - Uses long sync marks (40+ bits)
     * - Custom sector interleave
     * - Specific sync patterns per version
     */
    
    /* Check for long sync marks */
    size_t long_sync_count = 0;
    size_t max_sync_len = 0;
    
    for (size_t bit = 0; bit + 100 < gcr_bits; ) {
        if (get_bit(gcr_data, bit)) {
            uint32_t sync_len = count_sync_run(gcr_data, gcr_bits, bit);
            if (sync_len > max_sync_len) max_sync_len = sync_len;
            if (sync_len >= C64_SYNC_LONG_BITS) long_sync_count++;
            bit += sync_len;
        } else {
            bit++;
        }
    }
    
    /* V-MAX! typically has 5-10 long sync marks per track */
    if (long_sync_count >= 5 && long_sync_count <= 15) {
        confidence += 25;
    }
    
    /* V-MAX! v1 signature: 0x55 0xAA pattern in header */
    int32_t v1_sig = find_pattern_bits(gcr_data, gcr_bits, 0, 
                                        0x55AA55AA, 32);
    if (v1_sig >= 0) {
        confidence += 30;
        scheme->id = UFT_PROT_C64_VMAX_V1;
    }
    
    /* V-MAX! v2 signature: Different header structure */
    /* Look for the V-MAX! signature bytes after sync */
    size_t sig_matches = 0;
    for (size_t bit = 0; bit + 50 < gcr_bits; bit++) {
        if (count_sync_run(gcr_data, gcr_bits, bit) >= 30) {
            /* Check bytes after sync for V-MAX! pattern */
            uint16_t post_sync = get_word_be(gcr_data, bit + 40);
            if ((post_sync & 0xFF00) == 0x5200 ||
                (post_sync & 0xFF00) == 0x5500) {
                sig_matches++;
            }
        }
    }
    
    if (sig_matches >= 3) {
        confidence += 25;
        if (scheme->id == 0) {
            scheme->id = UFT_PROT_C64_VMAX_V2;
        }
    }
    
    /* V-MAX! v3 detection: More complex encoding */
    /* Look for density switching indicators */
    size_t zone_anomalies = 0;
    uint32_t expected_bits = (track <= 17) ? C64_NOMINAL_TRACK_BITS_ZONE0 :
                             (track <= 24) ? C64_NOMINAL_TRACK_BITS_ZONE1 :
                             (track <= 30) ? C64_NOMINAL_TRACK_BITS_ZONE2 :
                                             C64_NOMINAL_TRACK_BITS_ZONE3;
    
    uint32_t track_percent = (gcr_bits * 100) / expected_bits;
    if (track_percent > 110 || track_percent < 90) {
        zone_anomalies++;
        confidence += 10;
    }
    
    /* Finalize detection */
    if (confidence >= 50) {
        if (scheme->id == 0) {
            scheme->id = UFT_PROT_C64_VMAX_GENERIC;
        }
        scheme->confidence = confidence;
        scheme->platform = UFT_PLATFORM_C64;
        scheme->indicator_count = 1;
        scheme->indicators[0].type = UFT_IND_SYNC_LENGTH;
        scheme->indicators[0].value = max_sync_len;
        scheme->indicators[0].confidence = confidence;
        
        snprintf(scheme->notes, sizeof(scheme->notes),
                 "V-MAX! detected: %zu long syncs, max %zu bits",
                 long_sync_count, max_sync_len);
    }
    
    return confidence;
}

/**
 * @brief Detect RapidLok protection and determine version
 */
uint8_t uft_prot_c64_detect_rapidlok(
    const uint8_t* gcr_data,
    size_t gcr_bits,
    uint8_t track,
    uft_prot_scheme_t* scheme)
{
    if (!gcr_data || gcr_bits < 1000 || !scheme) {
        return 0;
    }
    
    memset(scheme, 0, sizeof(*scheme));
    uint8_t confidence = 0;
    
    /* RapidLok characteristics:
     * - Track 36 (half-track 36.0) contains protection
     * - Uses illegal GCR bytes
     * - Specific sync byte patterns
     * - Versions 1-4 have distinct signatures
     */
    
    /* Count illegal GCR patterns */
    size_t illegal_gcr_count = 0;
    size_t pos = 0;
    
    while (pos + 10 < gcr_bits) {
        /* Extract 5-bit GCR groups */
        uint8_t gcr5 = 0;
        for (int i = 0; i < 5; i++) {
            gcr5 = (gcr5 << 1) | get_bit(gcr_data, pos + i);
        }
        
        if (!is_valid_gcr(gcr5)) {
            illegal_gcr_count++;
        }
        pos += 5;
    }
    
    /* RapidLok uses many illegal GCR bytes */
    if (illegal_gcr_count > 50) {
        confidence += 30;
    } else if (illegal_gcr_count > 20) {
        confidence += 15;
    }
    
    /* RapidLok v1 signature: Specific header pattern */
    int32_t v1_pos = find_pattern_bits(gcr_data, gcr_bits, 0,
                                        0x52414C, 24); /* "RAL" in some form */
    if (v1_pos >= 0) {
        confidence += 25;
        scheme->id = UFT_PROT_C64_RAPIDLOK_V1;
    }
    
    /* RapidLok v2-v4: Look for version-specific markers */
    /* V2: More aggressive timing */
    /* V3: Added half-track checks */
    /* V4: Enhanced anti-analysis */
    
    /* Check for half-track data (common in RapidLok v3+) */
    if (track * 2 == 71 || track * 2 == 72) { /* Track 35.5 or 36 */
        confidence += 20;
        if (scheme->id == 0) {
            scheme->id = UFT_PROT_C64_RAPIDLOK_V3;
        }
    }
    
    /* Look for specific GCR sync pattern */
    int32_t sync_pattern = find_pattern_bits(gcr_data, gcr_bits, 0,
                                              0xFFFFFC00, 24);
    if (sync_pattern >= 0) {
        confidence += 15;
    }
    
    /* Finalize */
    if (confidence >= 40) {
        if (scheme->id == 0) {
            scheme->id = UFT_PROT_C64_RAPIDLOK_GENERIC;
        }
        scheme->confidence = confidence;
        scheme->platform = UFT_PLATFORM_C64;
        scheme->indicator_count = 1;
        scheme->indicators[0].type = UFT_IND_ILLEGAL_ENCODING;
        scheme->indicators[0].value = illegal_gcr_count;
        scheme->indicators[0].confidence = confidence;
        
        snprintf(scheme->notes, sizeof(scheme->notes),
                 "RapidLok detected: %zu illegal GCR bytes",
                 illegal_gcr_count);
    }
    
    return confidence;
}

/**
 * @brief Detect Vorpal protection and determine version
 */
uint8_t uft_prot_c64_detect_vorpal(
    const uint8_t* gcr_data,
    size_t gcr_bits,
    uint8_t track,
    uft_prot_scheme_t* scheme)
{
    if (!gcr_data || gcr_bits < 1000 || !scheme) {
        return 0;
    }
    
    memset(scheme, 0, sizeof(*scheme));
    uint8_t confidence = 0;
    
    /* Vorpal characteristics:
     * - Fast loader with protection on track 18
     * - Uses variable sync lengths
     * - Specific sector interleave pattern
     * - CRC-based verification
     */
    
    /* Count sync patterns and their lengths */
    size_t sync_count = 0;
    uint32_t sync_lengths[64];
    size_t unique_lengths = 0;
    
    for (size_t bit = 0; bit + 20 < gcr_bits && sync_count < 64; ) {
        if (get_bit(gcr_data, bit)) {
            uint32_t len = count_sync_run(gcr_data, gcr_bits, bit);
            if (len >= C64_SYNC_MIN_BITS) {
                /* Check if this length is unique */
                bool found = false;
                for (size_t i = 0; i < unique_lengths; i++) {
                    if (sync_lengths[i] == len) {
                        found = true;
                        break;
                    }
                }
                if (!found && unique_lengths < 64) {
                    sync_lengths[unique_lengths++] = len;
                }
                sync_count++;
            }
            bit += len;
        } else {
            bit++;
        }
    }
    
    /* Vorpal uses multiple distinct sync lengths */
    if (unique_lengths >= 3 && unique_lengths <= 8) {
        confidence += 30;
    }
    
    /* Look for Vorpal signature pattern */
    /* Vorpal typically starts with specific header */
    int32_t vorpal_sig = find_pattern_bits(gcr_data, gcr_bits, 0,
                                            0x564F5250, 32); /* "VORP" */
    if (vorpal_sig >= 0) {
        confidence += 35;
        scheme->id = UFT_PROT_C64_VORPAL_V1;
    }
    
    /* Check for Vorpal v2 characteristics */
    /* V2 adds additional timing checks */
    if (track == 18 && sync_count >= 20) {
        confidence += 15;
        if (scheme->id == 0) {
            scheme->id = UFT_PROT_C64_VORPAL_V2;
        }
    }
    
    /* Finalize */
    if (confidence >= 40) {
        if (scheme->id == 0) {
            scheme->id = UFT_PROT_C64_VORPAL_GENERIC;
        }
        scheme->confidence = confidence;
        scheme->platform = UFT_PLATFORM_C64;
        scheme->indicator_count = 1;
        scheme->indicators[0].type = UFT_IND_SYNC_LENGTH;
        scheme->indicators[0].value = unique_lengths;
        scheme->indicators[0].confidence = confidence;
        
        snprintf(scheme->notes, sizeof(scheme->notes),
                 "Vorpal detected: %zu unique sync lengths",
                 unique_lengths);
    }
    
    return confidence;
}

/**
 * @brief Detect Fat Track protection
 */
uint8_t uft_prot_c64_detect_fat_track(
    size_t track_bits,
    uint8_t track,
    uft_prot_scheme_t* scheme)
{
    if (!scheme) return 0;
    
    memset(scheme, 0, sizeof(*scheme));
    
    /* Fat track = significantly more data than normal */
    uint32_t expected_bits = (track <= 17) ? C64_NOMINAL_TRACK_BITS_ZONE0 :
                             (track <= 24) ? C64_NOMINAL_TRACK_BITS_ZONE1 :
                             (track <= 30) ? C64_NOMINAL_TRACK_BITS_ZONE2 :
                                             C64_NOMINAL_TRACK_BITS_ZONE3;
    
    uint32_t percent = (track_bits * 100) / expected_bits;
    
    if (percent >= C64_LONG_TRACK_THRESHOLD) {
        scheme->id = UFT_PROT_C64_FAT_TRACK;
        scheme->confidence = (percent > 120) ? 95 : 
                             (percent > 110) ? 80 : 65;
        scheme->platform = UFT_PLATFORM_C64;
        scheme->indicator_count = 1;
        scheme->indicators[0].type = UFT_IND_TRACK_LENGTH;
        scheme->indicators[0].value = track_bits;
        scheme->indicators[0].expected = expected_bits;
        scheme->indicators[0].confidence = scheme->confidence;
        
        snprintf(scheme->notes, sizeof(scheme->notes),
                 "Fat Track: %zu bits (%u%% of normal)",
                 track_bits, percent);
        
        return scheme->confidence;
    }
    
    return 0;
}

/**
 * @brief Detect GCR Timing Variations
 */
uint8_t uft_prot_c64_detect_timing(
    const uint32_t* bitcell_times,
    size_t sample_count,
    uint32_t nominal_time_ns,
    uft_prot_scheme_t* scheme)
{
    if (!bitcell_times || sample_count < 100 || !scheme) {
        return 0;
    }
    
    memset(scheme, 0, sizeof(*scheme));
    
    /* Calculate timing statistics */
    double sum = 0;
    double sum_sq = 0;
    uint32_t min_time = UINT32_MAX;
    uint32_t max_time = 0;
    
    for (size_t i = 0; i < sample_count; i++) {
        uint32_t t = bitcell_times[i];
        sum += t;
        sum_sq += (double)t * t;
        if (t < min_time) min_time = t;
        if (t > max_time) max_time = t;
    }
    
    double mean = sum / sample_count;
    double variance = (sum_sq / sample_count) - (mean * mean);
    double stddev = sqrt(variance);
    
    /* Calculate coefficient of variation */
    double cv = (stddev / mean) * 100.0;
    
    /* Timing protection has high variance */
    uint8_t confidence = 0;
    
    if (cv > 15.0) {
        confidence = 90;  /* Very high variance */
    } else if (cv > 10.0) {
        confidence = 75;
    } else if (cv > 5.0) {
        confidence = 50;
    }
    
    /* Check for intentional timing variations */
    uint32_t range = max_time - min_time;
    uint32_t tolerance = nominal_time_ns / 10;  /* 10% tolerance */
    
    if (range > tolerance * 3) {
        confidence += 15;
        if (confidence > 100) confidence = 100;
    }
    
    if (confidence >= 40) {
        scheme->id = UFT_PROT_C64_GCR_TIMING;
        scheme->confidence = confidence;
        scheme->platform = UFT_PLATFORM_C64;
        scheme->indicator_count = 1;
        scheme->indicators[0].type = UFT_IND_TIMING_VARIATION;
        scheme->indicators[0].value = (uint32_t)(cv * 100);  /* CV * 100 */
        scheme->indicators[0].confidence = confidence;
        
        snprintf(scheme->notes, sizeof(scheme->notes),
                 "GCR Timing: CV=%.2f%%, range=%u-%u ns",
                 cv, min_time, max_time);
        
        return confidence;
    }
    
    return 0;
}

/*============================================================================
 * APPLE II PROTECTION DETECTION (S-002)
 *============================================================================*/

/**
 * @brief Detect Nibble Count protection
 */
uint8_t uft_prot_apple_detect_nibble_count(
    const uint8_t* track_data,
    size_t track_size,
    uint8_t track,
    uft_prot_scheme_t* scheme)
{
    if (!track_data || track_size < 100 || !scheme) {
        return 0;
    }
    
    memset(scheme, 0, sizeof(*scheme));
    
    /* Nibble count protection: track has specific number of bytes */
    /* Standard Apple II track is ~6656 bytes */
    int32_t deviation = (int32_t)track_size - APPLE_TRACK_BYTES;
    uint32_t abs_deviation = (deviation < 0) ? -deviation : deviation;
    
    uint8_t confidence = 0;
    
    /* Check for non-standard nibble count */
    if (abs_deviation > APPLE_NIBBLE_COUNT_TOLERANCE * 4) {
        confidence = 90;  /* Very unusual count */
    } else if (abs_deviation > APPLE_NIBBLE_COUNT_TOLERANCE * 2) {
        confidence = 70;
    } else if (abs_deviation > APPLE_NIBBLE_COUNT_TOLERANCE) {
        confidence = 50;
    }
    
    /* Look for protection code patterns */
    /* Nibble count checks often use LDA/CMP sequences */
    size_t pattern_matches = 0;
    for (size_t i = 0; i + 3 < track_size; i++) {
        /* Look for potential count check code */
        if (track_data[i] == 0xA9 &&     /* LDA #imm */
            track_data[i+2] == 0xC9) {   /* CMP #imm */
            pattern_matches++;
        }
    }
    
    if (pattern_matches > 0) {
        confidence += 10;
        if (confidence > 100) confidence = 100;
    }
    
    if (confidence >= 40) {
        scheme->id = UFT_PROT_APPLE_NIBBLE_COUNT;
        scheme->confidence = confidence;
        scheme->platform = UFT_PLATFORM_APPLE_II;
        scheme->indicator_count = 1;
        scheme->indicators[0].type = UFT_IND_SECTOR_COUNT;
        scheme->indicators[0].value = track_size;
        scheme->indicators[0].expected = APPLE_TRACK_BYTES;
        scheme->indicators[0].confidence = confidence;
        
        snprintf(scheme->notes, sizeof(scheme->notes),
                 "Nibble Count: %zu bytes (deviation=%d)",
                 track_size, deviation);
        
        return confidence;
    }
    
    return 0;
}

/**
 * @brief Detect Timing Bit protection
 */
uint8_t uft_prot_apple_detect_timing(
    const uint8_t* flux_data,
    size_t flux_count,
    uint8_t track,
    uft_prot_scheme_t* scheme)
{
    if (!flux_data || flux_count < 100 || !scheme) {
        return 0;
    }
    
    memset(scheme, 0, sizeof(*scheme));
    
    /* Timing bit protection: specific timing patterns in flux */
    /* Look for intentional timing variations */
    
    size_t long_gaps = 0;
    size_t short_gaps = 0;
    size_t normal_gaps = 0;
    
    /* Analyze flux intervals */
    /* Assuming flux_data contains timing values */
    for (size_t i = 0; i < flux_count; i++) {
        uint8_t timing = flux_data[i];
        
        if (timing > 56) {         /* Long gap (>4us at 14MHz) */
            long_gaps++;
        } else if (timing < 48) {  /* Short gap (<3.5us) */
            short_gaps++;
        } else {
            normal_gaps++;
        }
    }
    
    /* Timing protection has unusual gap distribution */
    uint8_t confidence = 0;
    
    double long_ratio = (double)long_gaps / flux_count;
    double short_ratio = (double)short_gaps / flux_count;
    
    /* Intentional timing has ~10-20% unusual gaps */
    if (long_ratio > 0.15 || short_ratio > 0.15) {
        confidence = 85;
    } else if (long_ratio > 0.10 || short_ratio > 0.10) {
        confidence = 65;
    } else if (long_ratio > 0.05 || short_ratio > 0.05) {
        confidence = 45;
    }
    
    if (confidence >= 40) {
        scheme->id = UFT_PROT_APPLE_TIMING_BITS;
        scheme->confidence = confidence;
        scheme->platform = UFT_PLATFORM_APPLE_II;
        scheme->indicator_count = 2;
        
        scheme->indicators[0].type = UFT_IND_TIMING_VARIATION;
        scheme->indicators[0].value = (uint32_t)(long_ratio * 1000);
        scheme->indicators[0].confidence = confidence;
        
        scheme->indicators[1].type = UFT_IND_BITCELL_DEVIATION;
        scheme->indicators[1].value = (uint32_t)(short_ratio * 1000);
        scheme->indicators[1].confidence = confidence;
        
        snprintf(scheme->notes, sizeof(scheme->notes),
                 "Timing Bits: %.1f%% long, %.1f%% short gaps",
                 long_ratio * 100, short_ratio * 100);
        
        return confidence;
    }
    
    return 0;
}

/**
 * @brief Detect Spiral Track protection
 */
uint8_t uft_prot_apple_detect_spiral(
    const uint8_t** track_data,
    const size_t* track_sizes,
    uint8_t track_count,
    uft_prot_scheme_t* scheme)
{
    if (!track_data || !track_sizes || track_count < 2 || !scheme) {
        return 0;
    }
    
    memset(scheme, 0, sizeof(*scheme));
    
    /* Spiral track: data spans multiple tracks */
    /* Look for cross-track references */
    
    size_t cross_references = 0;
    
    for (uint8_t t = 0; t + 1 < track_count; t++) {
        if (!track_data[t] || !track_data[t+1]) continue;
        if (track_sizes[t] < 16 || track_sizes[t+1] < 16) continue;
        
        /* Check if end of track t matches start of track t+1 */
        size_t check_len = (track_sizes[t] < 16) ? track_sizes[t] : 16;
        
        bool match = true;
        for (size_t i = 0; i < check_len && match; i++) {
            size_t idx_a = track_sizes[t] - check_len + i;
            if (track_data[t][idx_a] != track_data[t+1][i]) {
                match = false;
            }
        }
        
        if (match) {
            cross_references++;
        }
    }
    
    uint8_t confidence = 0;
    
    if (cross_references > 5) {
        confidence = 90;
    } else if (cross_references > 2) {
        confidence = 70;
    } else if (cross_references > 0) {
        confidence = 45;
    }
    
    if (confidence >= 40) {
        scheme->id = UFT_PROT_APPLE_SPIRAL_TRACK;
        scheme->confidence = confidence;
        scheme->platform = UFT_PLATFORM_APPLE_II;
        scheme->indicator_count = 1;
        scheme->indicators[0].type = UFT_IND_CROSS_TRACK_SYNC;
        scheme->indicators[0].value = cross_references;
        scheme->indicators[0].confidence = confidence;
        
        snprintf(scheme->notes, sizeof(scheme->notes),
                 "Spiral Track: %zu cross-track references",
                 cross_references);
        
        return confidence;
    }
    
    return 0;
}

/**
 * @brief Detect Cross-Track Sync protection
 */
uint8_t uft_prot_apple_detect_cross_track(
    const uint8_t** track_data,
    const size_t* track_sizes,
    uint8_t track_count,
    uft_prot_scheme_t* scheme)
{
    if (!track_data || !track_sizes || track_count < 2 || !scheme) {
        return 0;
    }
    
    memset(scheme, 0, sizeof(*scheme));
    
    /* Cross-track sync: sync bytes span track boundaries */
    size_t sync_at_end = 0;
    size_t sync_at_start = 0;
    
    for (uint8_t t = 0; t < track_count; t++) {
        if (!track_data[t] || track_sizes[t] < 10) continue;
        
        /* Count sync bytes at track end */
        size_t end_sync = 0;
        for (size_t i = track_sizes[t]; i > 0 && 
             track_data[t][i-1] == APPLE_SYNC_BYTE; i--) {
            end_sync++;
        }
        if (end_sync >= 5) sync_at_end++;
        
        /* Count sync bytes at track start */
        size_t start_sync = 0;
        for (size_t i = 0; i < track_sizes[t] && 
             track_data[t][i] == APPLE_SYNC_BYTE; i++) {
            start_sync++;
        }
        if (start_sync >= 5) sync_at_start++;
    }
    
    /* Cross-track sync has many tracks with sync at boundaries */
    uint8_t confidence = 0;
    
    if (sync_at_end > 10 && sync_at_start > 10) {
        confidence = 85;
    } else if (sync_at_end > 5 || sync_at_start > 5) {
        confidence = 60;
    } else if (sync_at_end > 2 || sync_at_start > 2) {
        confidence = 40;
    }
    
    if (confidence >= 40) {
        scheme->id = UFT_PROT_APPLE_CROSS_TRACK;
        scheme->confidence = confidence;
        scheme->platform = UFT_PLATFORM_APPLE_II;
        scheme->indicator_count = 1;
        scheme->indicators[0].type = UFT_IND_CROSS_TRACK_SYNC;
        scheme->indicators[0].value = sync_at_end + sync_at_start;
        scheme->indicators[0].confidence = confidence;
        
        snprintf(scheme->notes, sizeof(scheme->notes),
                 "Cross-Track Sync: %zu end-sync, %zu start-sync tracks",
                 sync_at_end, sync_at_start);
        
        return confidence;
    }
    
    return 0;
}

/**
 * @brief Detect custom address/data marks
 */
uint8_t uft_prot_apple_detect_custom_marks(
    const uint8_t* track_data,
    size_t track_size,
    uft_prot_indicator_t* indicators,
    uint8_t max_indicators)
{
    if (!track_data || track_size < 10 || !indicators || max_indicators == 0) {
        return 0;
    }
    
    uint8_t found = 0;
    
    /* Look for non-standard prologues */
    for (size_t i = 0; i + 3 < track_size && found < max_indicators; i++) {
        /* Check for D5 AA xx pattern (standard is D5 AA 96 or D5 AA AD) */
        if (track_data[i] == APPLE_ADDRESS_PROLOGUE_D5 &&
            track_data[i+1] == APPLE_ADDRESS_PROLOGUE_AA) {
            
            uint8_t mark = track_data[i+2];
            
            /* Non-standard marks */
            if (mark != APPLE_ADDRESS_PROLOGUE_96 && 
                mark != APPLE_DATA_PROLOGUE_AD &&
                mark != 0xB5) {  /* Another common one */
                
                indicators[found].type = (mark < 0xA0) ? 
                    UFT_IND_ADDRESS_MARK : UFT_IND_DATA_MARK;
                indicators[found].location.track = 0;  /* Unknown */
                indicators[found].location.bit_offset = i;
                indicators[found].value = mark;
                indicators[found].confidence = 80;
                
                found++;
            }
        }
    }
    
    return found;
}

/*============================================================================
 * ATARI ST PROTECTION DETECTION (S-003)
 *============================================================================*/

/**
 * @brief Detect Copylock ST protection
 */
uint8_t uft_prot_atari_detect_copylock(
    const uint8_t* bitstream,
    uint32_t bit_count,
    uint8_t track,
    uft_prot_scheme_t* scheme)
{
    if (!bitstream || bit_count < 1000 || !scheme) {
        return 0;
    }
    
    memset(scheme, 0, sizeof(*scheme));
    
    /* Copylock ST characteristics:
     * - Located on track 79
     * - Uses custom sync words
     * - LFSR-based encryption
     * - Timing-critical sectors
     */
    
    uint8_t confidence = 0;
    
    /* Track 79 is typically the protection track */
    if (track == ATARI_COPYLOCK_TRACK) {
        confidence += 25;
    }
    
    /* Look for Copylock sync patterns */
    /* Copylock uses non-standard MFM sync words */
    size_t custom_sync_count = 0;
    
    for (size_t bit = 0; bit + 32 < bit_count; bit++) {
        uint16_t word = get_word_be(bitstream, bit);
        
        /* Check for Copylock-specific sync patterns */
        if ((word & 0xFFF0) == 0x4480 ||  /* Near-sync */
            (word & 0xFFF0) == 0x4490 ||
            word == 0x8914 ||              /* SLOW sector sync */
            word == 0x8912) {              /* FAST sector sync */
            custom_sync_count++;
        }
    }
    
    if (custom_sync_count > 10) {
        confidence += 40;
    } else if (custom_sync_count > 5) {
        confidence += 25;
    }
    
    /* Look for LFSR signature */
    /* Copylock uses specific LFSR polynomial */
    int32_t lfsr_sig = find_pattern_bits(bitstream, bit_count, 0,
                                          0x0001F041, 24);  /* LFSR tap pattern */
    if (lfsr_sig >= 0) {
        confidence += 20;
    }
    
    /* Determine version based on characteristics */
    if (confidence >= 50) {
        if (custom_sync_count > 15) {
            scheme->id = UFT_PROT_ATARI_COPYLOCK_V3;
        } else if (custom_sync_count > 8) {
            scheme->id = UFT_PROT_ATARI_COPYLOCK_V2;
        } else {
            scheme->id = UFT_PROT_ATARI_COPYLOCK_V1;
        }
        
        scheme->confidence = confidence;
        scheme->platform = UFT_PLATFORM_ATARI_ST;
        scheme->indicator_count = 1;
        scheme->indicators[0].type = UFT_IND_CUSTOM_SYNC;
        scheme->indicators[0].value = custom_sync_count;
        scheme->indicators[0].confidence = confidence;
        
        snprintf(scheme->notes, sizeof(scheme->notes),
                 "Copylock ST: %zu custom syncs on track %d",
                 custom_sync_count, track);
        
        return confidence;
    }
    
    return 0;
}

/**
 * @brief Detect Macrodos protection
 */
uint8_t uft_prot_atari_detect_macrodos(
    const uint8_t* bitstream,
    uint32_t bit_count,
    uint8_t track,
    uft_prot_scheme_t* scheme)
{
    if (!bitstream || bit_count < 1000 || !scheme) {
        return 0;
    }
    
    memset(scheme, 0, sizeof(*scheme));
    
    /* Macrodos characteristics:
     * - Uses 11 sectors per track instead of 9
     * - Sectors are 512 bytes (like standard)
     * - Requires longer tracks to fit extra sectors
     */
    
    uint8_t confidence = 0;
    
    /* Check track length - Macrodos tracks are longer */
    uint32_t track_bytes = bit_count / 16;  /* Approximate */
    
    if (track_bytes > ATARI_LONG_TRACK_BYTES) {
        confidence += 30;
    }
    
    /* Count MFM sync words */
    size_t sync_count = 0;
    
    for (size_t bit = 0; bit + 16 < bit_count; bit++) {
        if (get_word_be(bitstream, bit) == ATARI_MFM_SYNC_WORD) {
            sync_count++;
            bit += 16;  /* Skip this sync */
        }
    }
    
    /* Macrodos has 11 sectors (11 syncs expected) */
    if (sync_count >= 10 && sync_count <= 12) {
        confidence += 35;
        scheme->id = UFT_PROT_ATARI_MACRODOS;
    } else if (sync_count > 9) {
        confidence += 20;
    }
    
    /* Look for Macrodos+ signature */
    /* Macrodos+ adds additional protection markers */
    int32_t plus_sig = find_pattern_bits(bitstream, bit_count, 0,
                                          0x4D414352, 32);  /* "MACR" */
    if (plus_sig >= 0) {
        confidence += 25;
        scheme->id = UFT_PROT_ATARI_MACRODOS_PLUS;
    }
    
    if (confidence >= 40) {
        if (scheme->id == 0) {
            scheme->id = UFT_PROT_ATARI_MACRODOS;
        }
        scheme->confidence = confidence;
        scheme->platform = UFT_PLATFORM_ATARI_ST;
        scheme->indicator_count = 2;
        
        scheme->indicators[0].type = UFT_IND_SECTOR_COUNT;
        scheme->indicators[0].value = sync_count;
        scheme->indicators[0].expected = 9;
        scheme->indicators[0].confidence = confidence;
        
        scheme->indicators[1].type = UFT_IND_TRACK_LENGTH;
        scheme->indicators[1].value = track_bytes;
        scheme->indicators[1].expected = ATARI_NOMINAL_TRACK_BYTES;
        scheme->indicators[1].confidence = confidence;
        
        snprintf(scheme->notes, sizeof(scheme->notes),
                 "Macrodos: %zu sectors, %u bytes/track",
                 sync_count, track_bytes);
        
        return confidence;
    }
    
    return 0;
}

/**
 * @brief Detect Flaschel protection (FDC bug exploit)
 */
uint8_t uft_prot_atari_detect_flaschel(
    const uint8_t** sector_data,
    uint8_t sector_count,
    uft_prot_scheme_t* scheme)
{
    if (!sector_data || sector_count < 1 || !scheme) {
        return 0;
    }
    
    memset(scheme, 0, sizeof(*scheme));
    
    /* Flaschel exploits FDC timing bugs:
     * - Specific sector content patterns
     * - Relies on read timing variations
     * - Often uses sector interleave tricks
     */
    
    uint8_t confidence = 0;
    size_t flaschel_patterns = 0;
    
    for (uint8_t s = 0; s < sector_count; s++) {
        if (!sector_data[s]) continue;
        
        /* Look for Flaschel pattern markers */
        /* These are typically in the sector header area */
        
        /* Check for specific byte sequences */
        bool has_pattern = false;
        
        /* Flaschel uses patterns like 0x4E 0x4E followed by data */
        if (sector_data[s][0] == 0x4E && sector_data[s][1] == 0x4E) {
            has_pattern = true;
        }
        
        /* Also check for inverted patterns */
        if (sector_data[s][0] == 0xB1 && sector_data[s][1] == 0xB1) {
            has_pattern = true;
        }
        
        if (has_pattern) {
            flaschel_patterns++;
        }
    }
    
    /* Multiple sectors with Flaschel patterns */
    if (flaschel_patterns > 3) {
        confidence = 85;
    } else if (flaschel_patterns > 1) {
        confidence = 60;
    } else if (flaschel_patterns > 0) {
        confidence = 40;
    }
    
    if (confidence >= 40) {
        scheme->id = UFT_PROT_ATARI_FLASCHEL;
        scheme->confidence = confidence;
        scheme->platform = UFT_PLATFORM_ATARI_ST;
        scheme->indicator_count = 1;
        scheme->indicators[0].type = UFT_IND_SECTOR_INTERLEAVE;
        scheme->indicators[0].value = flaschel_patterns;
        scheme->indicators[0].confidence = confidence;
        
        snprintf(scheme->notes, sizeof(scheme->notes),
                 "Flaschel: %zu pattern matches",
                 flaschel_patterns);
        
        return confidence;
    }
    
    return 0;
}

/**
 * @brief Detect Fuzzy Sector protection
 */
uint8_t uft_prot_atari_detect_fuzzy(
    const uint8_t** sector_reads,
    uint8_t read_count,
    size_t sector_size,
    uft_prot_scheme_t* scheme)
{
    if (!sector_reads || read_count < 2 || sector_size < 1 || !scheme) {
        return 0;
    }
    
    memset(scheme, 0, sizeof(*scheme));
    
    /* Fuzzy sector: intentionally unstable bits */
    /* Compare multiple reads of same sector */
    
    size_t differing_bytes = 0;
    size_t max_diff_offset = 0;
    
    for (size_t i = 0; i < sector_size; i++) {
        uint8_t first = sector_reads[0][i];
        bool differs = false;
        
        for (uint8_t r = 1; r < read_count; r++) {
            if (sector_reads[r][i] != first) {
                differs = true;
                break;
            }
        }
        
        if (differs) {
            differing_bytes++;
            max_diff_offset = i;
        }
    }
    
    uint8_t confidence = 0;
    double diff_percent = (double)differing_bytes * 100 / sector_size;
    
    /* Fuzzy sectors have 1-10% unstable bytes */
    if (diff_percent > 0.5 && diff_percent < 15.0) {
        confidence = (diff_percent > 5.0) ? 90 : 
                     (diff_percent > 2.0) ? 75 : 55;
    }
    
    if (confidence >= 40) {
        scheme->id = UFT_PROT_ATARI_FUZZY_SECTOR;
        scheme->confidence = confidence;
        scheme->platform = UFT_PLATFORM_ATARI_ST;
        scheme->indicator_count = 1;
        scheme->indicators[0].type = UFT_IND_WEAK_BITS;
        scheme->indicators[0].value = differing_bytes;
        scheme->indicators[0].confidence = confidence;
        
        snprintf(scheme->notes, sizeof(scheme->notes),
                 "Fuzzy Sector: %zu/%zu bytes unstable (%.1f%%)",
                 differing_bytes, sector_size, diff_percent);
        
        return confidence;
    }
    
    return 0;
}

/**
 * @brief Detect Long Track protection
 */
uint8_t uft_prot_atari_detect_long_track(
    uint32_t track_length,
    uint32_t expected_length,
    uft_prot_scheme_t* scheme)
{
    if (!scheme) return 0;
    
    memset(scheme, 0, sizeof(*scheme));
    
    if (expected_length == 0) {
        expected_length = ATARI_NOMINAL_TRACK_BYTES;
    }
    
    uint32_t percent = (track_length * 100) / expected_length;
    uint8_t confidence = 0;
    
    if (track_length > ATARI_LONG_TRACK_BYTES) {
        /* Long track protection */
        if (percent > 110) {
            confidence = 95;
        } else if (percent > 105) {
            confidence = 80;
        } else {
            confidence = 60;
        }
        
        scheme->id = UFT_PROT_ATARI_LONG_TRACK;
        
    } else if (track_length < ATARI_SHORT_TRACK_BYTES) {
        /* Short track protection */
        if (percent < 90) {
            confidence = 95;
        } else if (percent < 95) {
            confidence = 75;
        } else {
            confidence = 55;
        }
        
        scheme->id = UFT_PROT_ATARI_SHORT_TRACK;
    }
    
    if (confidence >= 40) {
        scheme->confidence = confidence;
        scheme->platform = UFT_PLATFORM_ATARI_ST;
        scheme->indicator_count = 1;
        scheme->indicators[0].type = UFT_IND_TRACK_LENGTH;
        scheme->indicators[0].value = track_length;
        scheme->indicators[0].expected = expected_length;
        scheme->indicators[0].confidence = confidence;
        
        const char* type_str = (scheme->id == UFT_PROT_ATARI_LONG_TRACK) ?
                               "Long" : "Short";
        
        snprintf(scheme->notes, sizeof(scheme->notes),
                 "%s Track: %u bytes (%u%% of expected %u)",
                 type_str, track_length, percent, expected_length);
        
        return confidence;
    }
    
    return 0;
}

/*============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Get protection scheme name
 */
const char* uft_prot_scheme_name(uft_protection_scheme_t scheme)
{
    switch (scheme) {
        case UFT_PROT_NONE: return "None";
        
        /* C64 */
        case UFT_PROT_C64_VMAX_V1: return "V-MAX! v1";
        case UFT_PROT_C64_VMAX_V2: return "V-MAX! v2";
        case UFT_PROT_C64_VMAX_V3: return "V-MAX! v3";
        case UFT_PROT_C64_VMAX_GENERIC: return "V-MAX! (unknown version)";
        case UFT_PROT_C64_RAPIDLOK_V1: return "RapidLok v1";
        case UFT_PROT_C64_RAPIDLOK_V2: return "RapidLok v2";
        case UFT_PROT_C64_RAPIDLOK_V3: return "RapidLok v3";
        case UFT_PROT_C64_RAPIDLOK_V4: return "RapidLok v4";
        case UFT_PROT_C64_RAPIDLOK_GENERIC: return "RapidLok (unknown version)";
        case UFT_PROT_C64_VORPAL_V1: return "Vorpal v1";
        case UFT_PROT_C64_VORPAL_V2: return "Vorpal v2";
        case UFT_PROT_C64_VORPAL_GENERIC: return "Vorpal (unknown version)";
        case UFT_PROT_C64_PIRATESLAYER: return "PirateSlayer";
        case UFT_PROT_C64_FAT_TRACK: return "Fat Track";
        case UFT_PROT_C64_HALF_TRACK: return "Half Track";
        case UFT_PROT_C64_GCR_TIMING: return "GCR Timing";
        case UFT_PROT_C64_CUSTOM_SYNC: return "Custom Sync";
        case UFT_PROT_C64_SECTOR_GAP: return "Non-Standard Sector Gap";
        case UFT_PROT_C64_DENSITY_MISMATCH: return "Density Mismatch";
        
        /* Apple II */
        case UFT_PROT_APPLE_NIBBLE_COUNT: return "Nibble Count";
        case UFT_PROT_APPLE_TIMING_BITS: return "Timing Bits";
        case UFT_PROT_APPLE_SPIRAL_TRACK: return "Spiral Track";
        case UFT_PROT_APPLE_CROSS_TRACK: return "Cross-Track Sync";
        case UFT_PROT_APPLE_CUSTOM_ADDR: return "Custom Address Marks";
        case UFT_PROT_APPLE_CUSTOM_DATA: return "Custom Data Marks";
        case UFT_PROT_APPLE_HALF_TRACK: return "Half Track";
        case UFT_PROT_APPLE_QUARTER_TRACK: return "Quarter Track";
        case UFT_PROT_APPLE_BIT_SLIP: return "Bit Slip";
        case UFT_PROT_APPLE_SYNC_FLOOD: return "Sync Flood";
        
        /* Atari ST */
        case UFT_PROT_ATARI_COPYLOCK_V1: return "Copylock ST v1";
        case UFT_PROT_ATARI_COPYLOCK_V2: return "Copylock ST v2";
        case UFT_PROT_ATARI_COPYLOCK_V3: return "Copylock ST v3";
        case UFT_PROT_ATARI_COPYLOCK_GENERIC: return "Copylock ST (unknown version)";
        case UFT_PROT_ATARI_MACRODOS: return "Macrodos";
        case UFT_PROT_ATARI_MACRODOS_PLUS: return "Macrodos+";
        case UFT_PROT_ATARI_FLASCHEL: return "Flaschel";
        case UFT_PROT_ATARI_FUZZY_SECTOR: return "Fuzzy Sector";
        case UFT_PROT_ATARI_LONG_TRACK: return "Long Track";
        case UFT_PROT_ATARI_SHORT_TRACK: return "Short Track";
        case UFT_PROT_ATARI_EXTRA_SECTOR: return "Extra Sector";
        case UFT_PROT_ATARI_MISSING_SECTOR: return "Missing Sector";
        case UFT_PROT_ATARI_SECTOR_IN_GAP: return "Sector in Gap";
        case UFT_PROT_ATARI_DATA_IN_GAP: return "Data in Gap";
        case UFT_PROT_ATARI_WEAK_BITS: return "Weak Bits";
        
        /* Amiga */
        case UFT_PROT_AMIGA_COPYLOCK: return "Copylock (Amiga)";
        case UFT_PROT_AMIGA_SPEEDLOCK: return "Speedlock";
        case UFT_PROT_AMIGA_LONG_TRACK: return "Long Track (Amiga)";
        case UFT_PROT_AMIGA_SHORT_TRACK: return "Short Track (Amiga)";
        case UFT_PROT_AMIGA_CUSTOM_SYNC: return "Custom Sync (Amiga)";
        case UFT_PROT_AMIGA_VARIABLE_SYNC: return "Variable Sync";
        case UFT_PROT_AMIGA_WEAK_BITS: return "Weak Bits (Amiga)";
        case UFT_PROT_AMIGA_CAPS_SPS: return "CAPS/SPS Special";
        
        /* PC */
        case UFT_PROT_PC_WEAK_SECTOR: return "Weak Sector";
        case UFT_PROT_PC_FAT_TRICKS: return "FAT Tricks";
        case UFT_PROT_PC_EXTRA_SECTOR: return "Extra Sector";
        case UFT_PROT_PC_LONG_SECTOR: return "Long Sector";
        
        /* Generic */
        case UFT_PROT_GENERIC_WEAK_BITS: return "Generic Weak Bits";
        case UFT_PROT_GENERIC_LONG_TRACK: return "Generic Long Track";
        case UFT_PROT_GENERIC_TIMING: return "Generic Timing";
        case UFT_PROT_GENERIC_CUSTOM_FORMAT: return "Custom Format";
        
        default: return "Unknown";
    }
}

/**
 * @brief Get platform name
 */
const char* uft_prot_platform_name(uft_platform_t platform)
{
    switch (platform) {
        case UFT_PLATFORM_C64: return "Commodore 64";
        case UFT_PLATFORM_C128: return "Commodore 128";
        case UFT_PLATFORM_VIC20: return "VIC-20";
        case UFT_PLATFORM_PLUS4: return "Plus/4";
        case UFT_PLATFORM_AMIGA: return "Amiga";
        case UFT_PLATFORM_APPLE_II: return "Apple II";
        case UFT_PLATFORM_APPLE_III: return "Apple III";
        case UFT_PLATFORM_MAC: return "Macintosh";
        case UFT_PLATFORM_ATARI_ST: return "Atari ST";
        case UFT_PLATFORM_ATARI_8BIT: return "Atari 8-bit";
        case UFT_PLATFORM_PC_DOS: return "PC/DOS";
        case UFT_PLATFORM_PC_98: return "NEC PC-98";
        case UFT_PLATFORM_MSX: return "MSX";
        case UFT_PLATFORM_BBC: return "BBC Micro";
        case UFT_PLATFORM_SPECTRUM: return "ZX Spectrum";
        case UFT_PLATFORM_CPC: return "Amstrad CPC";
        case UFT_PLATFORM_TRS80: return "TRS-80";
        case UFT_PLATFORM_TI99: return "TI-99/4A";
        default: return "Unknown";
    }
}

/**
 * @brief Get indicator type name
 */
const char* uft_prot_indicator_name(uft_indicator_type_t type)
{
    switch (type) {
        case UFT_IND_TRACK_LENGTH: return "Track Length";
        case UFT_IND_SECTOR_COUNT: return "Sector Count";
        case UFT_IND_SECTOR_SIZE: return "Sector Size";
        case UFT_IND_SECTOR_GAP: return "Sector Gap";
        case UFT_IND_HALF_TRACK: return "Half Track";
        case UFT_IND_QUARTER_TRACK: return "Quarter Track";
        case UFT_IND_CUSTOM_SYNC: return "Custom Sync";
        case UFT_IND_SYNC_LENGTH: return "Sync Length";
        case UFT_IND_SYNC_POSITION: return "Sync Position";
        case UFT_IND_ADDRESS_MARK: return "Address Mark";
        case UFT_IND_DATA_MARK: return "Data Mark";
        case UFT_IND_ENCODING_MIX: return "Encoding Mix";
        case UFT_IND_TIMING_VARIATION: return "Timing Variation";
        case UFT_IND_BITCELL_DEVIATION: return "Bitcell Deviation";
        case UFT_IND_DENSITY_ZONE: return "Density Zone";
        case UFT_IND_WEAK_BITS: return "Weak Bits";
        case UFT_IND_UNSTABLE_DATA: return "Unstable Data";
        case UFT_IND_ILLEGAL_ENCODING: return "Illegal Encoding";
        case UFT_IND_CRC_ERROR: return "CRC Error";
        case UFT_IND_HEADER_ERROR: return "Header Error";
        case UFT_IND_DUPLICATE_SECTOR: return "Duplicate Sector";
        case UFT_IND_SECTOR_INTERLEAVE: return "Sector Interleave";
        case UFT_IND_CROSS_TRACK_SYNC: return "Cross-Track Sync";
        default: return "Unknown";
    }
}

/**
 * @brief Check if protection scheme is preservable
 */
bool uft_prot_is_preservable(uft_protection_scheme_t scheme)
{
    /* All schemes are preservable with proper flux capture */
    /* Some require special handling */
    switch (scheme) {
        case UFT_PROT_NONE:
            return true;
            
        /* These require flux-level preservation */
        case UFT_PROT_ATARI_FUZZY_SECTOR:
        case UFT_PROT_AMIGA_WEAK_BITS:
        case UFT_PROT_GENERIC_WEAK_BITS:
        case UFT_PROT_C64_GCR_TIMING:
        case UFT_PROT_APPLE_TIMING_BITS:
            return true;  /* Preservable with flux capture */
            
        default:
            return true;  /* All schemes are preservable */
    }
}

/**
 * @brief Get preservation recommendations
 */
const char* uft_prot_preservation_notes(uft_protection_scheme_t scheme)
{
    switch (scheme) {
        case UFT_PROT_NONE:
            return "Standard preservation methods apply.";
            
        case UFT_PROT_C64_VMAX_V1:
        case UFT_PROT_C64_VMAX_V2:
        case UFT_PROT_C64_VMAX_V3:
        case UFT_PROT_C64_VMAX_GENERIC:
            return "Capture multiple revolutions. Long sync regions must be preserved exactly.";
            
        case UFT_PROT_C64_RAPIDLOK_V1:
        case UFT_PROT_C64_RAPIDLOK_V2:
        case UFT_PROT_C64_RAPIDLOK_V3:
        case UFT_PROT_C64_RAPIDLOK_V4:
        case UFT_PROT_C64_RAPIDLOK_GENERIC:
            return "Include half-tracks. Preserve illegal GCR patterns.";
            
        case UFT_PROT_C64_FAT_TRACK:
            return "Ensure full track length is captured. May require slow read.";
            
        case UFT_PROT_C64_GCR_TIMING:
            return "Use flux-level capture. Timing variations must be preserved.";
            
        case UFT_PROT_APPLE_NIBBLE_COUNT:
            return "Capture exact track length. Do not normalize.";
            
        case UFT_PROT_APPLE_TIMING_BITS:
            return "Use flux capture. Timing is critical.";
            
        case UFT_PROT_APPLE_SPIRAL_TRACK:
        case UFT_PROT_APPLE_CROSS_TRACK:
            return "Capture all tracks including unused ones. Check track boundaries.";
            
        case UFT_PROT_ATARI_COPYLOCK_V1:
        case UFT_PROT_ATARI_COPYLOCK_V2:
        case UFT_PROT_ATARI_COPYLOCK_V3:
        case UFT_PROT_ATARI_COPYLOCK_GENERIC:
            return "Track 79 is critical. Use flux capture for LFSR data.";
            
        case UFT_PROT_ATARI_MACRODOS:
        case UFT_PROT_ATARI_MACRODOS_PLUS:
            return "Capture full track length for 11 sectors.";
            
        case UFT_PROT_ATARI_FLASCHEL:
            return "Multiple reads required. FDC timing must be precise.";
            
        case UFT_PROT_ATARI_FUZZY_SECTOR:
            return "Multiple revolutions required. Store all variations.";
            
        case UFT_PROT_ATARI_LONG_TRACK:
        case UFT_PROT_ATARI_SHORT_TRACK:
            return "Do not normalize track length. Preserve exact size.";
            
        case UFT_PROT_AMIGA_COPYLOCK:
            return "Flux capture recommended. Preserve LFSR seed.";
            
        case UFT_PROT_AMIGA_WEAK_BITS:
            return "Multiple revolutions required. Use weak bit detection.";
            
        default:
            return "Use flux-level capture for best preservation.";
    }
}

/**
 * @brief Export protection analysis to JSON
 */
int uft_prot_export_json(
    const uft_prot_result_t* result,
    char* buffer,
    size_t buffer_size)
{
    if (!result || !buffer || buffer_size < 256) {
        return -1;
    }
    
    int written = snprintf(buffer, buffer_size,
        "{\n"
        "  \"platform\": \"%s\",\n"
        "  \"scheme_count\": %u,\n"
        "  \"primary_scheme\": \"%s\",\n"
        "  \"primary_confidence\": %u,\n"
        "  \"schemes\": [\n",
        uft_prot_platform_name(result->platform),
        (unsigned)result->scheme_count,
        (result->scheme_count > 0) ? 
            uft_prot_scheme_name(result->schemes[0].id) : "None",
        (result->scheme_count > 0) ? (unsigned)result->schemes[0].confidence : 0u
    );
    
    if (written < 0 || (size_t)written >= buffer_size) {
        return -1;
    }
    
    size_t pos = written;
    
    for (uint8_t i = 0; i < result->scheme_count && i < UFT_PROT_MAX_SCHEMES; i++) {
        const uft_prot_scheme_t* s = &result->schemes[i];
        
        int n = snprintf(buffer + pos, buffer_size - pos,
            "    {\n"
            "      \"id\": \"0x%04X\",\n"
            "      \"name\": \"%s\",\n"
            "      \"confidence\": %u,\n"
            "      \"notes\": \"%s\"\n"
            "    }%s\n",
            s->id,
            uft_prot_scheme_name(s->id),
            (unsigned)s->confidence,
            s->notes,
            (i + 1 < result->scheme_count) ? "," : ""
        );
        
        if (n < 0 || pos + n >= buffer_size) break;
        pos += n;
    }
    
    int final = snprintf(buffer + pos, buffer_size - pos,
        "  ]\n"
        "}\n"
    );
    
    if (final < 0) return -1;
    
    return (int)(pos + final);
}

/**
 * @brief Export protection analysis to Markdown
 */
int uft_prot_export_markdown(
    const uft_prot_result_t* result,
    char* buffer,
    size_t buffer_size)
{
    if (!result || !buffer || buffer_size < 256) {
        return -1;
    }
    
    int written = snprintf(buffer, buffer_size,
        "# Protection Analysis Report\n\n"
        "**Platform:** %s  \n"
        "**Schemes Detected:** %u  \n\n"
        "## Detected Schemes\n\n"
        "| Scheme | Confidence | Notes |\n"
        "|--------|------------|-------|\n",
        uft_prot_platform_name(result->platform),
        result->scheme_count
    );
    
    if (written < 0 || (size_t)written >= buffer_size) {
        return -1;
    }
    
    size_t pos = written;
    
    for (uint8_t i = 0; i < result->scheme_count && i < UFT_PROT_MAX_SCHEMES; i++) {
        const uft_prot_scheme_t* s = &result->schemes[i];
        
        int n = snprintf(buffer + pos, buffer_size - pos,
            "| %s | %u%% | %s |\n",
            uft_prot_scheme_name(s->id),
            s->confidence,
            s->notes
        );
        
        if (n < 0 || pos + n >= buffer_size) break;
        pos += n;
    }
    
    int final = snprintf(buffer + pos, buffer_size - pos,
        "\n## Preservation Recommendations\n\n"
    );
    
    if (final < 0) return -1;
    pos += final;
    
    for (uint8_t i = 0; i < result->scheme_count && i < UFT_PROT_MAX_SCHEMES; i++) {
        const char* notes = uft_prot_preservation_notes(result->schemes[i].id);
        
        int n = snprintf(buffer + pos, buffer_size - pos,
            "- **%s:** %s\n",
            uft_prot_scheme_name(result->schemes[i].id),
            notes
        );
        
        if (n < 0 || pos + n >= buffer_size) break;
        pos += n;
    }
    
    return (int)pos;
}

/**
 * @brief Print protection analysis summary
 */
void uft_prot_print_summary(const uft_prot_result_t* result)
{
    if (!result) return;
    
    printf("\n=== Protection Analysis Summary ===\n");
    printf("Platform: %s\n", uft_prot_platform_name(result->platform));
    printf("Schemes detected: %u\n\n", (unsigned)result->scheme_count);
    
    for (uint8_t i = 0; i < result->scheme_count && i < UFT_PROT_MAX_SCHEMES; i++) {
        const uft_prot_scheme_t* s = &result->schemes[i];
        
        printf("[%u] %s (0x%04X)\n", (unsigned)(i + 1), uft_prot_scheme_name(s->id), s->id);
        printf("    Confidence: %u%%\n", (unsigned)s->confidence);
        printf("    Indicators: %u\n", (unsigned)s->indicator_count);
        
        for (uint8_t j = 0; j < s->indicator_count && j < UFT_PROT_SCHEME_MAX_INDICATORS; j++) {
            printf("      - %s: %u", 
                   uft_prot_indicator_name(s->indicators[j].type),
                   s->indicators[j].value);
            if (s->indicators[j].expected > 0) {
                printf(" (expected: %u)", s->indicators[j].expected);
            }
            printf("\n");
        }
        
        if (s->notes[0]) {
            printf("    Notes: %s\n", s->notes);
        }
        
        printf("    Preservation: %s\n\n", 
               uft_prot_preservation_notes(s->id));
    }
    
    printf("===================================\n\n");
}
