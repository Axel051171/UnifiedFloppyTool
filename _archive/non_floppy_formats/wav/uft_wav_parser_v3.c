/**
 * @file uft_wav_parser_v3.c
 * @brief GOD MODE WAV Parser v3 - Audio WAV for Tape Analysis
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define WAV_RIFF_MAGIC          "RIFF"
#define WAV_WAVE_MAGIC          "WAVE"

typedef struct {
    uint32_t file_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    uint32_t data_size;
    size_t source_size;
    bool valid;
} wav_file_t;

static uint32_t wav_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static uint16_t wav_read_le16(const uint8_t* p) {
    return p[0] | (p[1] << 8);
}

static bool wav_parse(const uint8_t* data, size_t size, wav_file_t* wav) {
    if (!data || !wav || size < 44) return false;
    memset(wav, 0, sizeof(wav_file_t));
    wav->source_size = size;
    
    if (memcmp(data, WAV_RIFF_MAGIC, 4) != 0) return false;
    if (memcmp(data + 8, WAV_WAVE_MAGIC, 4) != 0) return false;
    
    wav->file_size = wav_read_le32(data + 4);
    
    /* Find fmt chunk */
    if (memcmp(data + 12, "fmt ", 4) == 0) {
        wav->audio_format = wav_read_le16(data + 20);
        wav->num_channels = wav_read_le16(data + 22);
        wav->sample_rate = wav_read_le32(data + 24);
        wav->byte_rate = wav_read_le32(data + 28);
        wav->block_align = wav_read_le16(data + 32);
        wav->bits_per_sample = wav_read_le16(data + 34);
    }
    
    wav->valid = true;
    return true;
}

#ifdef WAV_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== WAV Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t wav[64] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' '};
    wav_file_t file;
    assert(wav_parse(wav, sizeof(wav), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
