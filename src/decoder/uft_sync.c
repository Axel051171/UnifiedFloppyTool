/**
 * @file uft_sync.c
 * @brief Sync Finder Implementation
 */

#include "uft/decoder/uft_sync.h"
#include <string.h>

const uft_sync_pattern_t UFT_SYNC_MFM_IDAM = {0xA1A1A1FE, 32, 1, 0};
const uft_sync_pattern_t UFT_SYNC_MFM_DAM = {0xA1A1A1FB, 32, 1, 0};
const uft_sync_pattern_t UFT_SYNC_GCR_HEADER = {0xFFFFFFFFFF, 40, 1, 0};

static uint64_t get_bits(const uint8_t* data, size_t pos, int count) {
    uint64_t result = 0;
    for (int i = 0; i < count; i++) {
        int idx = (pos + i) / 8;
        int bit = 7 - ((pos + i) % 8);
        result = (result << 1) | ((data[idx] >> bit) & 1);
    }
    return result;
}

int uft_sync_hamming(uint64_t a, uint64_t b, int bits) {
    uint64_t diff = a ^ b;
    int count = 0;
    for (int i = 0; i < bits; i++) {
        if (diff & (1ULL << i)) count++;
    }
    return count;
}

int uft_sync_find(const uint8_t* data, size_t bit_count, const uft_sync_pattern_t* pattern,
                  uft_sync_match_t* matches, int max_matches) {
    return uft_sync_find_fuzzy(data, bit_count, pattern, 0, matches, max_matches);
}

int uft_sync_find_fuzzy(const uint8_t* data, size_t bit_count, const uft_sync_pattern_t* pattern,
                        int max_hamming, uft_sync_match_t* matches, int max_matches) {
    if (!data || !pattern || !matches) return 0;
    
    int count = 0;
    
    for (size_t pos = 0; pos + pattern->pattern_bits <= bit_count && count < max_matches; pos++) {
        uint64_t found = get_bits(data, pos, pattern->pattern_bits);
        int dist = uft_sync_hamming(found, pattern->pattern, pattern->pattern_bits);
        
        if (dist <= max_hamming) {
            matches[count].position = pos;
            matches[count].hamming_distance = dist;
            matches[count].is_fuzzy = (dist > 0);
            matches[count].confidence = 1.0 - (double)dist / pattern->pattern_bits;
            count++;
        }
    }
    
    return count;
}
