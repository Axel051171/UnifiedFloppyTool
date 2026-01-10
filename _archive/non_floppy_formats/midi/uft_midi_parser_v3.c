/**
 * @file uft_midi_parser_v3.c
 * @brief GOD MODE MIDI Parser v3 - MIDI Music
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define MIDI_MAGIC              "MThd"

typedef struct {
    char signature[5];
    uint32_t header_length;
    uint16_t format;
    uint16_t num_tracks;
    uint16_t division;
    size_t source_size;
    bool valid;
} midi_file_t;

static uint32_t midi_read_be32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static uint16_t midi_read_be16(const uint8_t* p) {
    return (p[0] << 8) | p[1];
}

static bool midi_parse(const uint8_t* data, size_t size, midi_file_t* midi) {
    if (!data || !midi || size < 14) return false;
    memset(midi, 0, sizeof(midi_file_t));
    midi->source_size = size;
    
    if (memcmp(data, MIDI_MAGIC, 4) == 0) {
        memcpy(midi->signature, data, 4);
        midi->signature[4] = '\0';
        midi->header_length = midi_read_be32(data + 4);
        midi->format = midi_read_be16(data + 8);
        midi->num_tracks = midi_read_be16(data + 10);
        midi->division = midi_read_be16(data + 12);
        midi->valid = true;
    }
    return true;
}

#ifdef MIDI_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== MIDI Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t midi[16] = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, 4};
    midi_file_t file;
    assert(midi_parse(midi, sizeof(midi), &file) && file.num_tracks == 4);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
