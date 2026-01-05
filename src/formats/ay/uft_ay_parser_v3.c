/**
 * @file uft_ay_parser_v3.c
 * @brief GOD MODE AY Parser v3 - AY-3-8910 Music
 * 
 * ZX Spectrum/Amstrad/Atari ST music
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define AY_MAGIC                "ZXAYEMUL"

typedef struct {
    char signature[9];
    uint8_t file_version;
    uint8_t player_version;
    uint16_t spec_player_offset;
    uint16_t author_offset;
    uint16_t misc_offset;
    uint8_t num_songs;
    uint8_t first_song;
    size_t source_size;
    bool valid;
} ay_file_t;

static bool ay_parse(const uint8_t* data, size_t size, ay_file_t* ay) {
    if (!data || !ay || size < 20) return false;
    memset(ay, 0, sizeof(ay_file_t));
    ay->source_size = size;
    
    if (memcmp(data, AY_MAGIC, 8) == 0) {
        memcpy(ay->signature, data, 8);
        ay->signature[8] = '\0';
        ay->file_version = data[8];
        ay->player_version = data[9];
        ay->num_songs = data[16] + 1;
        ay->first_song = data[17];
        ay->valid = true;
    }
    return true;
}

#ifdef AY_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== AY Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t ay[32] = {'Z', 'X', 'A', 'Y', 'E', 'M', 'U', 'L', 0, 1};
    ay[16] = 4;  /* 5 songs */
    ay_file_t file;
    assert(ay_parse(ay, sizeof(ay), &file) && file.num_songs == 5);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
