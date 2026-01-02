/**
 * @file fuzz_fusion.c
 * @brief Fuzz target for multi-revolution fusion
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    uint8_t value;
    float confidence;
    bool weak;
} fused_bit_t;

void fuse_bits(const uint8_t** revs, int num_revs, size_t num_bits,
               fused_bit_t* out) {
    for (size_t i = 0; i < num_bits; i++) {
        int ones = 0;
        for (int r = 0; r < num_revs; r++) {
            if (revs[r][i / 8] & (1 << (7 - i % 8))) {
                ones++;
            }
        }
        
        out[i].value = (ones > num_revs / 2) ? 1 : 0;
        out[i].confidence = (float)(ones > num_revs - ones ? ones : num_revs - ones) / num_revs;
        out[i].weak = (out[i].confidence < 0.8f);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 10) return 0;
    
    // Use first byte as num_revs (2-8)
    int num_revs = (data[0] % 7) + 2;
    size_t bytes_per_rev = (size - 1) / num_revs;
    if (bytes_per_rev < 1) return 0;
    
    const uint8_t* revs[8];
    for (int r = 0; r < num_revs; r++) {
        revs[r] = data + 1 + r * bytes_per_rev;
    }
    
    size_t num_bits = bytes_per_rev * 8;
    fused_bit_t* result = malloc(num_bits * sizeof(fused_bit_t));
    if (!result) return 0;
    
    fuse_bits(revs, num_revs, num_bits, result);
    
    // Invariant checks
    for (size_t i = 0; i < num_bits; i++) {
        if (result[i].value > 1) {
            free(result);
            __builtin_trap();
        }
        if (result[i].confidence < 0 || result[i].confidence > 1.0f) {
            free(result);
            __builtin_trap();
        }
    }
    
    free(result);
    return 0;
}
