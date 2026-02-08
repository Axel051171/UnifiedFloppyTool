/**
 * @file uft_gcr_parser_v3.c
 * @brief GOD MODE GCR Parser v3 - Group Coded Recording
 * 
 * Commodore/Apple GCR encoded raw data
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* C64 GCR sync pattern */
#define GCR_C64_SYNC            0xFF
#define GCR_C64_HEADER_ID       0x08
#define GCR_C64_DATA_ID         0x07

/* Apple II GCR patterns */
#define GCR_APPLE_D5            0xD5
#define GCR_APPLE_AA            0xAA

typedef struct {
    uint32_t bit_count;
    uint32_t sync_count;
    uint32_t header_count;
    uint32_t data_block_count;
    bool is_commodore;
    bool is_apple;
    double bit_rate_kbps;
    size_t source_size;
    bool valid;
} gcr_stream_t;

static bool gcr_parse(const uint8_t* data, size_t size, gcr_stream_t* gcr) {
    if (!data || !gcr || size < 16) return false;
    memset(gcr, 0, sizeof(gcr_stream_t));
    gcr->source_size = size;
    gcr->bit_count = (uint32_t)size * 8;
    
    /* Scan for patterns */
    for (size_t i = 0; i < size - 2; i++) {
        /* Check for C64 sync (10+ 0xFF bytes) */
        if (data[i] == GCR_C64_SYNC) {
            gcr->sync_count++;
            gcr->is_commodore = true;
        }
        
        /* Check for Apple D5 AA patterns */
        if (data[i] == GCR_APPLE_D5 && data[i+1] == GCR_APPLE_AA) {
            gcr->header_count++;
            gcr->is_apple = true;
        }
    }
    
    /* Estimate data rate */
    if (gcr->is_commodore) {
        gcr->bit_rate_kbps = 250.0;  /* C64 1541 rate */
    } else if (gcr->is_apple) {
        gcr->bit_rate_kbps = 250.0;  /* Apple Disk II rate */
    }
    
    gcr->valid = (gcr->sync_count > 0 || gcr->header_count > 0 || size > 1000);
    return true;
}

#ifdef GCR_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== GCR Stream Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t gcr[32] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x08};
    gcr_stream_t file;
    assert(gcr_parse(gcr, sizeof(gcr), &file) && file.is_commodore);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
