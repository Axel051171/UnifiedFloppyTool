/**
 * @file uft_it_parser_v3.c
 * @brief GOD MODE IT Parser v3 - Impulse Tracker Module
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define IT_MAGIC                "IMPM"

typedef struct {
    char signature[5];
    char title[27];
    uint16_t order_count;
    uint16_t instrument_count;
    uint16_t sample_count;
    uint16_t pattern_count;
    uint16_t tracker_version;
    uint16_t compatible_version;
    uint16_t flags;
    uint8_t global_volume;
    uint8_t mix_volume;
    uint8_t initial_speed;
    uint8_t initial_tempo;
    size_t source_size;
    bool valid;
} it_file_t;

static bool it_parse(const uint8_t* data, size_t size, it_file_t* it) {
    if (!data || !it || size < 192) return false;
    memset(it, 0, sizeof(it_file_t));
    it->source_size = size;
    
    if (memcmp(data, IT_MAGIC, 4) == 0) {
        memcpy(it->signature, data, 4);
        it->signature[4] = '\0';
        memcpy(it->title, data + 4, 26);
        it->title[26] = '\0';
        it->order_count = data[32] | (data[33] << 8);
        it->instrument_count = data[34] | (data[35] << 8);
        it->sample_count = data[36] | (data[37] << 8);
        it->pattern_count = data[38] | (data[39] << 8);
        it->tracker_version = data[40] | (data[41] << 8);
        it->global_volume = data[48];
        it->mix_volume = data[49];
        it->initial_speed = data[50];
        it->initial_tempo = data[51];
        it->valid = true;
    }
    return true;
}

#ifdef IT_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== IT Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t it[192] = {0};
    memcpy(it, "IMPM", 4);
    memcpy(it + 4, "Test Song", 9);
    it_file_t file;
    assert(it_parse(it, sizeof(it), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
