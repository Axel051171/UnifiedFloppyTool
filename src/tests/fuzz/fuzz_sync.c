/**
 * @file fuzz_sync.c
 * @brief Fuzz target for sync pattern detection
 * 
 * Build with: clang -g -fsanitize=fuzzer,address fuzz_sync.c \
 *             ../algorithms/sync/uft_sync_detector.c -lm -o fuzz_sync
 */

#include "../algorithms/sync/uft_sync_detector.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4) return 0;
    
    uft_sync_detector_t det;
    uft_sync_init(&det);
    
    /* Configure from fuzz data */
    double expected_gap = 500 + (data[0] * 4);  /* 500-1500 bits */
    double tolerance = 0.1 + (data[1] / 255.0) * 0.4;  /* 10-50% */
    
    uft_sync_configure(&det, expected_gap, tolerance);
    
    /* Enable strict mode randomly */
    det.strict_mode = (data[2] & 0x80) != 0;
    
    /* Feed bytes to detector */
    uft_sync_candidate_t syncs[8];
    
    for (size_t i = 3; i < size; i++) {
        int count = uft_sync_feed_byte(&det, data[i], syncs);
        
        /* Invariant checks */
        if (count < 0 || count > 8) {
            abort();  /* Bug: impossible sync count */
        }
        
        if (det.candidate_count > UFT_SYNC_MAX_CANDIDATES) {
            abort();  /* Bug: buffer overflow */
        }
        
        /* Check all candidates have valid confidence */
        for (size_t c = 0; c < det.candidate_count; c++) {
            if (det.candidates[c].confidence > 100) {
                abort();  /* Bug: confidence > 100 */
            }
        }
    }
    
    /* Check best candidate logic */
    const uft_sync_candidate_t *best = uft_sync_get_best(&det);
    if (best && det.candidate_count == 0) {
        abort();  /* Bug: best returned when no candidates */
    }
    
    /* Cleanup */
    uft_sync_reset(&det);
    
    return 0;
}
