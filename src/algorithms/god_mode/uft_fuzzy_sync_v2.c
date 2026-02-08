/**
 * @file uft_fuzzy_sync_v2.c
 * @brief GOD MODE: Fuzzy Sync Pattern Detection
 * 
 * Finds sync patterns with bit errors using Hamming distance.
 * Supports MFM, FM, and GCR sync marks.
 * 
 * @author Superman QA - GOD MODE
 * @date 2026
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

// ============================================================================
// Sync Pattern Definitions
// ============================================================================

// MFM Sync: 0xA1 with missing clock = 0x4489
#define SYNC_MFM_A1     0x4489
#define SYNC_MFM_C2     0x5224  // 0xC2 with missing clock (Index AM)

// FM Sync
#define SYNC_FM_FE      0xF57E  // Address Mark
#define SYNC_FM_FB      0xF56F  // Data Mark

// GCR Commodore Sync
#define SYNC_GCR_CBM    0xFF    // 10 consecutive 1s = 0x3FF

// Apple GCR Sync
#define SYNC_GCR_APPLE  0xD5AA96  // Prologue bytes

// Maximum Hamming distance for "fuzzy" match
#define MAX_HAMMING_EXACT   0   // Exact match
#define MAX_HAMMING_FUZZY   2   // Up to 2 bit errors per word
#define MAX_HAMMING_LOOSE   4   // Last resort

// ============================================================================
// Types
// ============================================================================

typedef struct {
    size_t bit_position;    // Position in bitstream
    int confidence;         // 0-100 (100 = exact match)
    int hamming_distance;   // Total bit errors
    int bit_slip;           // Detected bit slip (-2 to +2)
    uint8_t sync_type;      // Which sync pattern found
} sync_match_t;

typedef struct {
    sync_match_t* matches;
    size_t count;
    size_t capacity;
    
    // Statistics
    int exact_matches;
    int fuzzy_matches;
    int loose_matches;
    double avg_confidence;
} sync_search_result_t;

typedef enum {
    SYNC_TYPE_MFM_A1,
    SYNC_TYPE_MFM_C2,
    SYNC_TYPE_FM_FE,
    SYNC_TYPE_FM_FB,
    SYNC_TYPE_GCR_CBM,
    SYNC_TYPE_GCR_APPLE
} sync_type_t;

// ============================================================================
// Hamming Distance
// ============================================================================

static inline int hamming16(uint16_t a, uint16_t b) {
    uint16_t diff = a ^ b;
    // Population count (portable)
    diff = diff - ((diff >> 1) & 0x5555);
    diff = (diff & 0x3333) + ((diff >> 2) & 0x3333);
    diff = (diff + (diff >> 4)) & 0x0F0F;
    return (diff + (diff >> 8)) & 0x1F;
}

static inline int hamming8(uint8_t a, uint8_t b) {
    uint8_t diff = a ^ b;
    diff = diff - ((diff >> 1) & 0x55);
    diff = (diff & 0x33) + ((diff >> 2) & 0x33);
    return (diff + (diff >> 4)) & 0x0F;
}

// ============================================================================
// Bitstream Access
// ============================================================================

static inline uint16_t get_bits16(const uint8_t* data, size_t bit_pos) {
    size_t byte_pos = bit_pos / 8;
    int bit_offset = bit_pos % 8;
    
    uint32_t window = ((uint32_t)data[byte_pos] << 16) |
                      ((uint32_t)data[byte_pos + 1] << 8) |
                      data[byte_pos + 2];
    
    return (window >> (8 - bit_offset)) & 0xFFFF;
}

static inline uint8_t get_bits8(const uint8_t* data, size_t bit_pos) {
    size_t byte_pos = bit_pos / 8;
    int bit_offset = bit_pos % 8;
    
    uint16_t window = ((uint16_t)data[byte_pos] << 8) | data[byte_pos + 1];
    
    return (window >> (8 - bit_offset)) & 0xFF;
}

// ============================================================================
// MFM Sync Search
// ============================================================================

/**
 * @brief Find MFM A1A1A1 sync pattern with fuzzy matching
 * 
 * Searches for three consecutive 0x4489 patterns (MFM-encoded 0xA1
 * with missing clock bit).
 * 
 * @param data Bitstream data
 * @param num_bits Total bits in stream
 * @param max_hamming Maximum allowed Hamming distance per sync word
 * @param results Output array (caller allocates)
 * @param max_results Maximum results to return
 * @return Number of syncs found
 */
size_t find_mfm_sync_fuzzy(const uint8_t* data, size_t num_bits,
                            int max_hamming,
                            sync_match_t* results, size_t max_results) {
    if (!data || !results || num_bits < 64) return 0;
    
    size_t found = 0;
    size_t num_bytes = (num_bits + 7) / 8;
    (void)num_bytes; // Suppress unused warning
    
    // Need at least 48 bits (3 × 16) for sync
    for (size_t bit = 0; bit + 48 <= num_bits && found < max_results; bit++) {
        // Get three consecutive 16-bit words
        uint16_t w1 = get_bits16(data, bit);
        uint16_t w2 = get_bits16(data, bit + 16);
        uint16_t w3 = get_bits16(data, bit + 32);
        
        int d1 = hamming16(w1, SYNC_MFM_A1);
        int d2 = hamming16(w2, SYNC_MFM_A1);
        int d3 = hamming16(w3, SYNC_MFM_A1);
        
        int total_dist = d1 + d2 + d3;
        
        if (total_dist <= max_hamming * 3) {
            sync_match_t* m = &results[found++];
            m->bit_position = bit;
            m->hamming_distance = total_dist;
            m->sync_type = SYNC_TYPE_MFM_A1;
            m->bit_slip = 0;
            
            // Confidence: 100% at 0 errors, drops 8% per bit error
            m->confidence = 100 - (total_dist * 8);
            if (m->confidence < 0) m->confidence = 0;
        }
    }
    
    return found;
}

/**
 * @brief Find MFM sync with bit-slip detection
 * 
 * Tries shifts of -2 to +2 bits to find better sync alignment.
 */
size_t find_mfm_sync_with_slip(const uint8_t* data, size_t num_bits,
                                sync_match_t* results, size_t max_results) {
    if (!data || !results || num_bits < 64) return 0;
    
    size_t found = 0;
    
    // First pass: exact matches
    found = find_mfm_sync_fuzzy(data, num_bits, MAX_HAMMING_EXACT, 
                                results, max_results);
    
    // If no exact matches, try fuzzy
    if (found == 0) {
        found = find_mfm_sync_fuzzy(data, num_bits, MAX_HAMMING_FUZZY,
                                    results, max_results);
        
        // Try bit-slip correction for each match
        for (size_t i = 0; i < found; i++) {
            sync_match_t* m = &results[i];
            int best_dist = m->hamming_distance;
            int best_slip = 0;
            
            // Try shifts
            for (int slip = -2; slip <= 2; slip++) {
                if (slip == 0) continue;
                
                size_t test_bit = m->bit_position + slip;
                if (test_bit >= num_bits - 48) continue;
                
                uint16_t w1 = get_bits16(data, test_bit);
                uint16_t w2 = get_bits16(data, test_bit + 16);
                uint16_t w3 = get_bits16(data, test_bit + 32);
                
                int total = hamming16(w1, SYNC_MFM_A1) +
                           hamming16(w2, SYNC_MFM_A1) +
                           hamming16(w3, SYNC_MFM_A1);
                
                if (total < best_dist) {
                    best_dist = total;
                    best_slip = slip;
                }
            }
            
            if (best_slip != 0) {
                m->bit_position += best_slip;
                m->bit_slip = best_slip;
                m->hamming_distance = best_dist;
                m->confidence = 100 - (best_dist * 8);
            }
        }
    }
    
    return found;
}

// ============================================================================
// GCR Sync Search (Commodore)
// ============================================================================

/**
 * @brief Find GCR sync for Commodore formats
 * 
 * GCR sync is 5+ consecutive 0xFF bytes (40+ set bits).
 */
size_t find_gcr_sync_cbm(const uint8_t* data, size_t num_bits,
                          sync_match_t* results, size_t max_results) {
    if (!data || !results || num_bits < 80) return 0;
    
    size_t found = 0;
    size_t num_bytes = num_bits / 8;
    
    for (size_t byte = 0; byte + 5 <= num_bytes && found < max_results; byte++) {
        // Count consecutive 1 bits
        int ones = 0;
        for (size_t b = byte; b < byte + 5; b++) {
            if (data[b] == 0xFF) ones += 8;
            else break;
        }
        
        if (ones >= 40) {  // 5 bytes of 0xFF
            sync_match_t* m = &results[found++];
            m->bit_position = byte * 8;
            m->hamming_distance = 0;
            m->sync_type = SYNC_TYPE_GCR_CBM;
            m->bit_slip = 0;
            m->confidence = 100;
            
            // Skip past sync
            byte += 4;
        }
    }
    
    return found;
}

// ============================================================================
// Full Track Sync Analysis
// ============================================================================

typedef struct {
    int expected_syncs;
    int syncs_found;
    int exact_syncs;
    int fuzzy_syncs;
    int missed_syncs;
    
    double sync_rate;           // syncs_found / expected
    double exact_rate;          // exact / expected
    double avg_hamming;
    double avg_confidence;
    
    // Sector-level info
    int* sector_syncs;          // Per-sector sync count
    int num_sectors;
} sync_analysis_t;

void analyze_track_syncs(const uint8_t* data, size_t num_bits,
                          sync_type_t sync_type, int expected_syncs,
                          sync_analysis_t* analysis) {
    sync_match_t matches[256];
    size_t found;
    
    switch (sync_type) {
        case SYNC_TYPE_MFM_A1:
            found = find_mfm_sync_with_slip(data, num_bits, matches, 256);
            break;
        case SYNC_TYPE_GCR_CBM:
            found = find_gcr_sync_cbm(data, num_bits, matches, 256);
            break;
        default:
            found = 0;
    }
    
    analysis->expected_syncs = expected_syncs;
    analysis->syncs_found = (int)found;
    analysis->exact_syncs = 0;
    analysis->fuzzy_syncs = 0;
    
    double total_hamming = 0;
    double total_conf = 0;
    
    for (size_t i = 0; i < found; i++) {
        if (matches[i].hamming_distance == 0) {
            analysis->exact_syncs++;
        } else {
            analysis->fuzzy_syncs++;
        }
        total_hamming += matches[i].hamming_distance;
        total_conf += matches[i].confidence;
    }
    
    if (expected_syncs > 0) {
        analysis->sync_rate = (double)found / expected_syncs * 100;
        analysis->exact_rate = (double)analysis->exact_syncs / expected_syncs * 100;
    }
    
    if (found > 0) {
        analysis->avg_hamming = total_hamming / found;
        analysis->avg_confidence = total_conf / found;
    }
    
    analysis->missed_syncs = expected_syncs - (int)found;
    if (analysis->missed_syncs < 0) analysis->missed_syncs = 0;
}
