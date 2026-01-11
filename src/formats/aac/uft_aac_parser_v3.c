/**
 * @file uft_aac_parser_v3.c
 * @brief GOD MODE AAC Parser v3 - Advanced Audio Coding
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define ADTS_SYNC               0xFFF

typedef struct {
    uint16_t sync_word;
    uint8_t mpeg_version;    /* 0=MPEG-4, 1=MPEG-2 */
    uint8_t layer;
    uint8_t profile;
    uint8_t sampling_freq_idx;
    uint8_t channel_config;
    uint16_t frame_length;
    bool is_adts;
    bool is_adif;
    size_t source_size;
    bool valid;
} aac_file_t;

static const uint32_t aac_sample_rates[] = {
    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
    16000, 12000, 11025, 8000, 7350, 0, 0, 0
};

static bool aac_parse(const uint8_t* data, size_t size, aac_file_t* aac) {
    if (!data || !aac || size < 7) return false;
    memset(aac, 0, sizeof(aac_file_t));
    aac->source_size = size;
    
    /* Check for ADTS sync word */
    aac->sync_word = (data[0] << 4) | (data[1] >> 4);
    if (aac->sync_word == ADTS_SYNC) {
        aac->is_adts = true;
        aac->mpeg_version = (data[1] >> 3) & 0x01;
        aac->layer = (data[1] >> 1) & 0x03;
        aac->profile = (data[2] >> 6) & 0x03;
        aac->sampling_freq_idx = (data[2] >> 2) & 0x0F;
        aac->channel_config = ((data[2] & 0x01) << 2) | ((data[3] >> 6) & 0x03);
        aac->frame_length = ((data[3] & 0x03) << 11) | (data[4] << 3) | ((data[5] >> 5) & 0x07);
        aac->valid = true;
    }
    /* Check for ADIF header */
    else if (memcmp(data, "ADIF", 4) == 0) {
        aac->is_adif = true;
        aac->valid = true;
    }
    return true;
}

#ifdef AAC_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== AAC Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t aac[16] = {0xFF, 0xF1, 0x50, 0x80};  /* ADTS frame */
    aac_file_t file;
    assert(aac_parse(aac, sizeof(aac), &file) && file.is_adts);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
