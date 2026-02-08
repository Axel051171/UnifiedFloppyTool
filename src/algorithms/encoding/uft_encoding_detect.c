/**
 * @file uft_encoding_detect.c
 * @brief Automatic encoding detection implementation
 */

#include "uft_encoding_detect.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

/* MFM sync patterns */
#define MFM_SYNC_A1         0x4489  /* A1 with missing clock */
#define MFM_SYNC_C2         0x5224  /* C2 with missing clock */

/* Apple GCR patterns */
#define APPLE_SYNC_D5AA     0xD5AA
#define APPLE_SYNC_D5AAB5   0xD5AAB5  /* Sector header */
#define APPLE_SYNC_D5AAAD   0xD5AAAD  /* Data field */

/* Commodore GCR sync */
#define C64_SYNC_BYTE       0xFF    /* 10 consecutive 1s */

/* Encoding names */
static const char* encoding_names[] = {
    "Unknown",
    "FM",
    "MFM",
    "Apple GCR",
    "Commodore GCR",
    "Macintosh GCR",
    "Amiga MFM",
    "M2FM"
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static inline uint8_t get_bit(const uint8_t *data, size_t pos) {
    return (data[pos / 8] >> (7 - (pos % 8))) & 1;
}

static uint16_t get_word_be(const uint8_t *data, size_t byte_pos) {
    return ((uint16_t)data[byte_pos] << 8) | data[byte_pos + 1];
}

/* ============================================================================
 * Histogram Analysis
 * ============================================================================ */

void uft_encoding_build_histogram(const uint8_t *data,
                                  size_t len_bits,
                                  uft_pulse_histogram_t *histogram) {
    if (!data || !histogram || len_bits < 16) return;
    
    memset(histogram, 0, sizeof(*histogram));
    histogram->bucket_width = 1;  /* 1 sample per bucket */
    
    size_t last_pulse = 0;
    bool found_first = false;
    
    for (size_t i = 0; i < len_bits; i++) {
        if (get_bit(data, i)) {
            if (found_first) {
                size_t interval = i - last_pulse;
                if (interval < 64) {
                    histogram->buckets[interval]++;
                }
                histogram->total_pulses++;
            }
            last_pulse = i;
            found_first = true;
        }
    }
}

void uft_encoding_find_peaks(uft_pulse_histogram_t *histogram) {
    if (!histogram) return;
    
    histogram->peak_count = 0;
    
    /* Simple peak detection: local maxima */
    for (size_t i = 2; i < 62 && histogram->peak_count < 8; i++) {
        size_t val = histogram->buckets[i];
        size_t left = histogram->buckets[i-1] + histogram->buckets[i-2];
        size_t right = histogram->buckets[i+1] + histogram->buckets[i+2];
        
        /* Is this a peak? */
        if (val > left / 2 && val > right / 2 && val > histogram->total_pulses / 50) {
            histogram->peak_positions[histogram->peak_count++] = i;
        }
    }
}

/* ============================================================================
 * Sync Pattern Detection
 * ============================================================================ */

size_t uft_encoding_find_syncs(const uint8_t *data,
                               size_t len_bits,
                               uft_encoding_type_t encoding) {
    if (!data || len_bits < 32) return 0;
    
    size_t count = 0;
    size_t len_bytes = len_bits / 8;
    
    switch (encoding) {
        case UFT_ENCODING_MFM:
        case UFT_ENCODING_AMIGA_MFM:
            /* Search for MFM A1 sync (0x4489) */
            for (size_t i = 0; i + 16 < len_bits; i++) {
                uint16_t pattern = 0;
                for (int j = 0; j < 16; j++) {
                    pattern = (pattern << 1) | get_bit(data, i + j);
                }
                if (pattern == MFM_SYNC_A1) {
                    count++;
                    i += 15;  /* Skip past this sync */
                }
            }
            break;
            
        case UFT_ENCODING_GCR_APPLE:
            /* Search for D5 AA pattern */
            for (size_t i = 0; i + 2 < len_bytes; i++) {
                if (data[i] == 0xD5 && data[i+1] == 0xAA) {
                    count++;
                }
            }
            break;
            
        case UFT_ENCODING_GCR_C64:
            /* Search for 10+ consecutive 1s */
            {
                int ones = 0;
                for (size_t i = 0; i < len_bits; i++) {
                    if (get_bit(data, i)) {
                        ones++;
                        if (ones >= 10) {
                            count++;
                            ones = 0;
                        }
                    } else {
                        ones = 0;
                    }
                }
            }
            break;
            
        case UFT_ENCODING_FM:
            /* FM uses clock bits, look for clock/data pattern */
            /* Clock bits at even positions */
            {
                int clock_count = 0;
                for (size_t i = 0; i + 2 < len_bits; i += 2) {
                    if (get_bit(data, i)) {
                        clock_count++;
                    }
                }
                /* FM should have ~50% clock bits */
                double ratio = (double)clock_count / (len_bits / 2);
                if (ratio > 0.45 && ratio < 0.55) {
                    count = 100;  /* Synthetic count */
                }
            }
            break;
            
        default:
            break;
    }
    
    return count;
}

/* ============================================================================
 * Scoring Functions
 * ============================================================================ */

static int score_mfm(const uint8_t *data, size_t len_bits,
                     const uft_pulse_histogram_t *hist) {
    int score = 0;
    
    /* MFM: 3 pulse intervals (2T, 3T, 4T) */
    if (hist->peak_count >= 2 && hist->peak_count <= 4) {
        score += 30;
    }
    
    /* Check for ratios ~2:3:4 */
    if (hist->peak_count >= 3) {
        double p1 = hist->peak_positions[0];
        double p2 = hist->peak_positions[1];
        double p3 = hist->peak_positions[2];
        
        double ratio1 = p2 / p1;
        double ratio2 = p3 / p1;
        
        if (ratio1 > 1.3 && ratio1 < 1.7) score += 20;  /* ~1.5 */
        if (ratio2 > 1.8 && ratio2 < 2.2) score += 20;  /* ~2.0 */
    }
    
    /* Check for A1 sync patterns */
    size_t syncs = uft_encoding_find_syncs(data, len_bits, UFT_ENCODING_MFM);
    if (syncs > 0) score += 30;
    if (syncs > 10) score += 20;
    
    return score;
}

static int score_gcr_apple(const uint8_t *data, size_t len_bits,
                           const uft_pulse_histogram_t *hist) {
    int score = 0;
    
    /* Apple GCR: 5 pulse intervals */
    if (hist->peak_count >= 4) {
        score += 20;
    }
    
    /* Check for D5 AA sync */
    size_t syncs = uft_encoding_find_syncs(data, len_bits, UFT_ENCODING_GCR_APPLE);
    if (syncs > 0) score += 40;
    if (syncs > 5) score += 20;
    
    return score;
}

static int score_gcr_c64(const uint8_t *data, size_t len_bits,
                         const uft_pulse_histogram_t *hist) {
    int score = 0;
    
    /* C64 GCR: 5 pulse intervals */
    if (hist->peak_count >= 4) {
        score += 20;
    }
    
    /* Check for sync patterns (10+ ones) */
    size_t syncs = uft_encoding_find_syncs(data, len_bits, UFT_ENCODING_GCR_C64);
    if (syncs > 5) score += 30;
    if (syncs > 15) score += 30;
    
    return score;
}

static int score_fm(const uint8_t *data, size_t len_bits,
                    const uft_pulse_histogram_t *hist) {
    int score = 0;
    
    /* FM: 2 pulse intervals (clock and clock+data) */
    if (hist->peak_count == 2) {
        score += 30;
    }
    
    /* Check ratio should be ~2:1 */
    if (hist->peak_count >= 2) {
        double ratio = (double)hist->peak_positions[1] / hist->peak_positions[0];
        if (ratio > 1.8 && ratio < 2.2) {
            score += 30;
        }
    }
    
    /* Check FM clock pattern */
    size_t result = uft_encoding_find_syncs(data, len_bits, UFT_ENCODING_FM);
    if (result > 50) score += 20;
    
    return score;
}

/* ============================================================================
 * Main Detection
 * ============================================================================ */

void uft_encoding_detect_all(const uint8_t *data,
                             size_t len,
                             double sample_rate,
                             uft_encoding_candidates_t *candidates) {
    if (!data || len == 0 || !candidates) return;
    
    memset(candidates, 0, sizeof(*candidates));
    
    size_t len_bits = len * 8;
    
    /* Build pulse histogram */
    uft_pulse_histogram_t hist;
    uft_encoding_build_histogram(data, len_bits, &hist);
    uft_encoding_find_peaks(&hist);
    
    candidates->total_pulses = hist.total_pulses;
    
    /* Score each encoding */
    candidates->results[UFT_ENCODING_FM].encoding = UFT_ENCODING_FM;
    candidates->results[UFT_ENCODING_FM].score = score_fm(data, len_bits, &hist);
    candidates->results[UFT_ENCODING_FM].name = encoding_names[UFT_ENCODING_FM];
    candidates->results[UFT_ENCODING_FM].bit_rate = 125e3;
    
    candidates->results[UFT_ENCODING_MFM].encoding = UFT_ENCODING_MFM;
    candidates->results[UFT_ENCODING_MFM].score = score_mfm(data, len_bits, &hist);
    candidates->results[UFT_ENCODING_MFM].name = encoding_names[UFT_ENCODING_MFM];
    candidates->results[UFT_ENCODING_MFM].bit_rate = 500e3;
    
    candidates->results[UFT_ENCODING_GCR_APPLE].encoding = UFT_ENCODING_GCR_APPLE;
    candidates->results[UFT_ENCODING_GCR_APPLE].score = score_gcr_apple(data, len_bits, &hist);
    candidates->results[UFT_ENCODING_GCR_APPLE].name = encoding_names[UFT_ENCODING_GCR_APPLE];
    candidates->results[UFT_ENCODING_GCR_APPLE].bit_rate = 250e3;
    
    candidates->results[UFT_ENCODING_GCR_C64].encoding = UFT_ENCODING_GCR_C64;
    candidates->results[UFT_ENCODING_GCR_C64].score = score_gcr_c64(data, len_bits, &hist);
    candidates->results[UFT_ENCODING_GCR_C64].name = encoding_names[UFT_ENCODING_GCR_C64];
    candidates->results[UFT_ENCODING_GCR_C64].bit_rate = 250e3;
    
    /* Amiga MFM (similar to regular MFM but different sync) */
    candidates->results[UFT_ENCODING_AMIGA_MFM].encoding = UFT_ENCODING_AMIGA_MFM;
    candidates->results[UFT_ENCODING_AMIGA_MFM].score = score_mfm(data, len_bits, &hist);
    candidates->results[UFT_ENCODING_AMIGA_MFM].name = encoding_names[UFT_ENCODING_AMIGA_MFM];
    candidates->results[UFT_ENCODING_AMIGA_MFM].bit_rate = 500e3;
    
    candidates->count = 5;
    
    /* Find best match */
    candidates->best = &candidates->results[0];
    for (size_t i = 1; i < candidates->count; i++) {
        if (candidates->results[i].score > candidates->best->score) {
            candidates->best = &candidates->results[i];
        }
    }
    
    /* Calculate cell size if we have sample rate */
    if (sample_rate > 0 && candidates->best->bit_rate > 0) {
        candidates->best->cell_size = sample_rate / candidates->best->bit_rate;
    }
}

uft_encoding_result_t uft_encoding_detect(const uint8_t *data,
                                          size_t len,
                                          double sample_rate) {
    uft_encoding_candidates_t candidates;
    uft_encoding_detect_all(data, len, sample_rate, &candidates);
    
    if (candidates.best) {
        return *candidates.best;
    }
    
    uft_encoding_result_t unknown = {0};
    unknown.encoding = UFT_ENCODING_UNKNOWN;
    unknown.name = encoding_names[UFT_ENCODING_UNKNOWN];
    return unknown;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* uft_encoding_name(uft_encoding_type_t encoding) {
    if (encoding < UFT_ENCODING_MAX) {
        return encoding_names[encoding];
    }
    return "Invalid";
}

double uft_encoding_typical_bitrate(uft_encoding_type_t encoding) {
    switch (encoding) {
        case UFT_ENCODING_FM:        return 125e3;
        case UFT_ENCODING_MFM:       return 500e3;
        case UFT_ENCODING_GCR_APPLE: return 250e3;
        case UFT_ENCODING_GCR_C64:   return 250e3;
        case UFT_ENCODING_GCR_MAC:   return 500e3;
        case UFT_ENCODING_AMIGA_MFM: return 500e3;
        case UFT_ENCODING_M2FM:      return 250e3;
        default:                     return 0;
    }
}

void uft_encoding_dump_results(const uft_encoding_candidates_t *candidates) {
    if (!candidates) {
        printf("Encoding Detection: NULL\n");
        return;
    }
    
    printf("=== Encoding Detection Results ===\n");
    printf("Total pulses analyzed: %zu\n\n", candidates->total_pulses);
    
    printf("Candidates (sorted by score):\n");
    for (size_t i = 0; i < candidates->count; i++) {
        const uft_encoding_result_t *r = &candidates->results[i];
        if (r->score > 0) {
            printf("  %s: score=%d", r->name, r->score);
            if (r == candidates->best) {
                printf(" [BEST]");
            }
            printf("\n");
        }
    }
    
    if (candidates->best) {
        printf("\nDetected: %s (score=%d, bitrate=%.0f Hz)\n",
               candidates->best->name,
               candidates->best->score,
               candidates->best->bit_rate);
    }
}
