/**
 * @file fuzz_weak_bits.c
 * @brief Fuzz target for weak bit handling
 * 
 * Build with: clang -g -fsanitize=fuzzer,address fuzz_weak_bits.c \
 *             ../algorithms/weak_bits/uft_weak_bits.c -lm -o fuzz_weak
 */

#include "../algorithms/weak_bits/uft_weak_bits.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 8) return 0;
    
    /* Test 1: Bit fusion */
    {
        uft_bit_fusion_t fusion;
        uft_fusion_clear(&fusion);
        
        size_t num_samples = (size / 2);
        if (num_samples > UFT_WEAK_MAX_REVISIONS) {
            num_samples = UFT_WEAK_MAX_REVISIONS;
        }
        
        for (size_t i = 0; i < num_samples; i++) {
            uint8_t value = data[i * 2] & 1;
            uint8_t conf = data[i * 2 + 1];
            uft_fusion_add_sample(&fusion, value, conf);
        }
        
        uft_prob_bit_t result = uft_fusion_fuse(&fusion);
        
        /* Invariants */
        if (result.value > 1) {
            abort();  /* Bug: bit value not 0 or 1 */
        }
        /* Confidence should be 0-255 - uint8_t guarantees this */
    }
    
    /* Test 2: Track operations */
    if (size >= 32) {
        size_t bit_count = data[0] * 8 + 64;  /* 64-2112 bits */
        
        uft_weak_track_t track;
        int rc = uft_weak_track_init(&track, bit_count);
        if (rc != 0) {
            return 0;  /* OOM is acceptable */
        }
        
        /* Set bits from fuzz data */
        for (size_t i = 1; i < size && (i-1) < bit_count; i++) {
            uint8_t value = data[i] & 1;
            uint8_t conf = data[i];
            uft_weak_track_set_bit(&track, i-1, value, conf);
        }
        
        /* Detect regions */
        size_t regions = uft_weak_track_detect_regions(&track, 4);
        
        /* Invariants */
        if (track.total_weak_bits + track.total_strong_bits > track.bit_count) {
            abort();  /* Bug: bit counts don't add up */
        }
        
        for (size_t r = 0; r < regions && r < track.region_count; r++) {
            if (track.regions[r].end_bit <= track.regions[r].start_bit) {
                abort();  /* Bug: invalid region bounds */
            }
            if (track.regions[r].end_bit > track.bit_count) {
                abort();  /* Bug: region extends past track */
            }
        }
        
        uft_weak_track_free(&track);
    }
    
    /* Test 3: Track comparison */
    if (size >= 64) {
        size_t bit_count = 256;
        
        uft_weak_track_t a, b;
        if (uft_weak_track_init(&a, bit_count) != 0) return 0;
        if (uft_weak_track_init(&b, bit_count) != 0) {
            uft_weak_track_free(&a);
            return 0;
        }
        
        /* Fill tracks */
        for (size_t i = 0; i < 32 && i < bit_count; i++) {
            uft_weak_track_set_bit(&a, i, data[i] & 1, data[i]);
            uft_weak_track_set_bit(&b, i, data[32 + i] & 1, data[32 + i]);
        }
        
        /* Compare */
        size_t diff_positions[256];
        size_t diffs = uft_weak_track_compare(&a, &b, diff_positions, 256);
        
        /* Diffs should be reasonable */
        if (diffs > bit_count) {
            abort();  /* Bug: more diffs than bits */
        }
        
        uft_weak_track_free(&a);
        uft_weak_track_free(&b);
    }
    
    return 0;
}
