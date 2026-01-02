/**
 * @file fuzz_fuzzy_sync.c
 * @brief Fuzz target for fuzzy sync detection
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define SYNC_MFM_A1 0x4489

static int hamming16(uint16_t a, uint16_t b) {
    uint16_t diff = a ^ b;
    int count = 0;
    while (diff) {
        count += diff & 1;
        diff >>= 1;
    }
    return count;
}

typedef struct {
    size_t position;
    int confidence;
    int hamming;
} sync_result_t;

sync_result_t find_sync(const uint8_t* data, size_t len) {
    sync_result_t best = {.position = (size_t)-1, .confidence = 0, .hamming = 99};
    
    if (len < 6) return best;
    
    for (size_t i = 0; i <= len - 6; i++) {
        uint16_t w1 = (data[i] << 8) | data[i+1];
        uint16_t w2 = (data[i+2] << 8) | data[i+3];
        uint16_t w3 = (data[i+4] << 8) | data[i+5];
        
        int d = hamming16(w1, SYNC_MFM_A1) +
                hamming16(w2, SYNC_MFM_A1) +
                hamming16(w3, SYNC_MFM_A1);
        
        if (d <= 6) {
            int conf = 100 - d * 8;
            if (conf > best.confidence) {
                best.position = i;
                best.confidence = conf;
                best.hamming = d;
            }
        }
    }
    
    return best;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 6) return 0;
    
    sync_result_t r = find_sync(data, size);
    
    // Invariant checks
    if (r.confidence > 0) {
        if (r.position > size - 6) {
            __builtin_trap();
        }
        if (r.confidence < 0 || r.confidence > 100) {
            __builtin_trap();
        }
        if (r.hamming < 0 || r.hamming > 48) {
            __builtin_trap();
        }
    }
    
    return 0;
}
