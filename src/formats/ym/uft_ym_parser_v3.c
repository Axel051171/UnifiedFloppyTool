/**
 * @file uft_ym_parser_v3.c
 * @brief GOD MODE YM Parser v3 - YM2149 Music
 * 
 * Atari ST music format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define YM_MAGIC_5              "YM5!"
#define YM_MAGIC_6              "YM6!"

typedef struct {
    char signature[5];
    uint8_t version;
    uint32_t num_frames;
    uint32_t attributes;
    uint16_t digidrums;
    uint32_t clock_freq;
    uint16_t player_freq;
    uint32_t loop_frame;
    char song_name[64];
    char author_name[64];
    size_t source_size;
    bool valid;
} ym_file_t;

static uint32_t ym_read_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | (p[2] << 8) | p[3];
}

static bool ym_parse(const uint8_t* data, size_t size, ym_file_t* ym) {
    if (!data || !ym || size < 34) return false;
    memset(ym, 0, sizeof(ym_file_t));
    ym->source_size = size;
    
    if (memcmp(data, YM_MAGIC_5, 4) == 0 || memcmp(data, YM_MAGIC_6, 4) == 0) {
        memcpy(ym->signature, data, 4);
        ym->signature[4] = '\0';
        ym->version = (data[2] == '5') ? 5 : 6;
        
        /* Check for "LeOnArD!" */
        if (memcmp(data + 4, "LeOnArD!", 8) == 0) {
            ym->num_frames = ym_read_be32(data + 12);
            ym->attributes = ym_read_be32(data + 16);
            ym->clock_freq = ym_read_be32(data + 24);
            ym->player_freq = (data[28] << 8) | data[29];
            ym->valid = true;
        }
    }
    return true;
}

#ifdef YM_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== YM Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t ym[64] = {'Y', 'M', '6', '!', 'L', 'e', 'O', 'n', 'A', 'r', 'D', '!'};
    ym_file_t file;
    assert(ym_parse(ym, sizeof(ym), &file) && file.version == 6);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
