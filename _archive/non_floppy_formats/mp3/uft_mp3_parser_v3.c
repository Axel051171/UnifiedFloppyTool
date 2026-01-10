/**
 * @file uft_mp3_parser_v3.c
 * @brief GOD MODE MP3 Parser v3 - MPEG Layer III Audio
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define ID3V2_MAGIC             "ID3"

typedef struct {
    bool has_id3v2;
    uint8_t id3v2_version;
    uint32_t id3v2_size;
    uint8_t mpeg_version;    /* 1, 2, or 2.5 */
    uint8_t layer;           /* 1, 2, or 3 */
    uint32_t bitrate;
    uint32_t sample_rate;
    uint8_t channel_mode;
    size_t source_size;
    bool valid;
} mp3_file_t;

static const uint32_t mp3_bitrates[16] = {
    0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0
};
static const uint32_t mp3_sample_rates[4] = {44100, 48000, 32000, 0};

static bool mp3_parse(const uint8_t* data, size_t size, mp3_file_t* mp3) {
    if (!data || !mp3 || size < 10) return false;
    memset(mp3, 0, sizeof(mp3_file_t));
    mp3->source_size = size;
    
    size_t offset = 0;
    
    /* Check for ID3v2 tag */
    if (memcmp(data, ID3V2_MAGIC, 3) == 0) {
        mp3->has_id3v2 = true;
        mp3->id3v2_version = data[3];
        mp3->id3v2_size = ((data[6] & 0x7F) << 21) | ((data[7] & 0x7F) << 14) |
                          ((data[8] & 0x7F) << 7) | (data[9] & 0x7F);
        offset = 10 + mp3->id3v2_size;
    }
    
    /* Find frame sync */
    while (offset + 4 < size) {
        if (data[offset] == 0xFF && (data[offset+1] & 0xE0) == 0xE0) {
            uint8_t version = (data[offset+1] >> 3) & 0x03;
            uint8_t layer = (data[offset+1] >> 1) & 0x03;
            uint8_t bitrate_idx = (data[offset+2] >> 4) & 0x0F;
            uint8_t sample_idx = (data[offset+2] >> 2) & 0x03;
            
            if (layer == 1) {  /* Layer 3 */
                mp3->mpeg_version = (version == 3) ? 1 : 2;
                mp3->layer = 3;
                mp3->bitrate = mp3_bitrates[bitrate_idx];
                mp3->sample_rate = mp3_sample_rates[sample_idx];
                mp3->channel_mode = (data[offset+3] >> 6) & 0x03;
                mp3->valid = true;
                break;
            }
        }
        offset++;
    }
    
    return true;
}

#ifdef MP3_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== MP3 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t mp3[32] = {'I', 'D', '3', 4, 0, 0, 0, 0, 0, 0, 0xFF, 0xFB, 0x90, 0x00};
    mp3_file_t file;
    assert(mp3_parse(mp3, sizeof(mp3), &file) && file.has_id3v2);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
