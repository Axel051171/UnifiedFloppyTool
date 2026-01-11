/**
 * @file uft_fm_parser_v3.c
 * @brief GOD MODE FM Parser v3 - Frequency Modulation Stream
 * 
 * Single-density FM encoded raw data (pre-MFM)
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* FM sync patterns */
#define FM_IAM_CLOCK            0xD7  /* Index Address Mark clock */
#define FM_IAM_DATA             0xFC  /* Index Address Mark data */
#define FM_IDAM_CLOCK           0xC7  /* ID Address Mark clock */
#define FM_IDAM_DATA            0xFE  /* ID Address Mark data */
#define FM_DAM_CLOCK            0xC7  /* Data Address Mark clock */
#define FM_DAM_DATA             0xFB  /* Data Address Mark data */

typedef struct {
    uint32_t bit_count;
    uint32_t byte_count;
    uint32_t mark_count;
    uint32_t sector_estimate;
    double data_rate_kbps;
    size_t source_size;
    bool valid;
} fm_stream_t;

static bool fm_parse(const uint8_t* data, size_t size, fm_stream_t* fm) {
    if (!data || !fm || size < 16) return false;
    memset(fm, 0, sizeof(fm_stream_t));
    fm->source_size = size;
    fm->byte_count = (uint32_t)size;
    fm->bit_count = fm->byte_count * 8;
    
    /* Scan for address marks */
    for (size_t i = 0; i < size; i++) {
        if (data[i] == FM_IDAM_DATA || data[i] == FM_DAM_DATA || 
            data[i] == FM_IAM_DATA) {
            fm->mark_count++;
        }
    }
    
    /* FM data rate is half of MFM (125 or 250 kbps) */
    fm->data_rate_kbps = 125.0;
    
    /* Estimate sectors */
    if (fm->mark_count >= 2) {
        fm->sector_estimate = fm->mark_count / 2;
    }
    
    fm->valid = (fm->mark_count > 0 || size > 500);
    return true;
}

#ifdef FM_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== FM Stream Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t fm[32] = {0x00, 0xFE, 0x00, 0x00, 0x01, 0x00, 0xF7, 0xFB};
    fm_stream_t file;
    assert(fm_parse(fm, sizeof(fm), &file) && file.mark_count > 0);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
