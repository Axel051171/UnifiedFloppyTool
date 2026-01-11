/**
 * @file uft_flac_parser_v3.c
 * @brief GOD MODE FLAC Parser v3 - Free Lossless Audio Codec
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define FLAC_MAGIC              "fLaC"

typedef struct {
    char signature[5];
    uint16_t min_block_size;
    uint16_t max_block_size;
    uint32_t min_frame_size;
    uint32_t max_frame_size;
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bits_per_sample;
    uint64_t total_samples;
    size_t source_size;
    bool valid;
} flac_file_t;

static bool flac_parse(const uint8_t* data, size_t size, flac_file_t* flac) {
    if (!data || !flac || size < 42) return false;
    memset(flac, 0, sizeof(flac_file_t));
    flac->source_size = size;
    
    if (memcmp(data, FLAC_MAGIC, 4) == 0) {
        memcpy(flac->signature, data, 4);
        flac->signature[4] = '\0';
        
        /* STREAMINFO block at offset 4 */
        if ((data[4] & 0x7F) == 0) {  /* STREAMINFO type */
            flac->min_block_size = (data[8] << 8) | data[9];
            flac->max_block_size = (data[10] << 8) | data[11];
            flac->sample_rate = (data[18] << 12) | (data[19] << 4) | (data[20] >> 4);
            flac->channels = ((data[20] >> 1) & 0x07) + 1;
            flac->bits_per_sample = ((data[20] & 1) << 4) | (data[21] >> 4) + 1;
        }
        flac->valid = true;
    }
    return true;
}

#ifdef FLAC_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== FLAC Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t flac[64] = {'f', 'L', 'a', 'C', 0x00, 0, 0, 34};
    flac_file_t file;
    assert(flac_parse(flac, sizeof(flac), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
