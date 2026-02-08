/**
 * @file uft_adx_parser_v3.c
 * @brief GOD MODE ADX Parser v3 - CRI ADX Audio
 * 
 * SEGA Dreamcast/Saturn audio format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define ADX_MAGIC               0x8000

typedef struct {
    uint16_t signature;
    uint16_t copyright_offset;
    uint8_t encoding_type;
    uint8_t block_size;
    uint8_t bit_depth;
    uint8_t channel_count;
    uint32_t sample_rate;
    uint32_t total_samples;
    uint16_t highpass_freq;
    uint8_t version;
    uint8_t flags;
    size_t source_size;
    bool valid;
} adx_file_t;

static bool adx_parse(const uint8_t* data, size_t size, adx_file_t* adx) {
    if (!data || !adx || size < 20) return false;
    memset(adx, 0, sizeof(adx_file_t));
    adx->source_size = size;
    
    adx->signature = (data[0] << 8) | data[1];
    if (adx->signature == ADX_MAGIC) {
        adx->copyright_offset = (data[2] << 8) | data[3];
        adx->encoding_type = data[4];
        adx->block_size = data[5];
        adx->bit_depth = data[6];
        adx->channel_count = data[7];
        adx->sample_rate = ((uint32_t)data[8] << 24) | ((uint32_t)data[9] << 16) | (data[10] << 8) | data[11];
        adx->total_samples = ((uint32_t)data[12] << 24) | ((uint32_t)data[13] << 16) | (data[14] << 8) | data[15];
        adx->highpass_freq = (data[16] << 8) | data[17];
        adx->version = data[18];
        adx->flags = data[19];
        adx->valid = true;
    }
    return true;
}

#ifdef ADX_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== ADX Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t adx[32] = {0x80, 0x00, 0x00, 0x20, 3, 18, 4, 2};
    adx[8] = 0x00; adx[9] = 0x00; adx[10] = 0xAC; adx[11] = 0x44;  /* 44100 Hz */
    adx_file_t file;
    assert(adx_parse(adx, sizeof(adx), &file) && file.sample_rate == 44100);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
