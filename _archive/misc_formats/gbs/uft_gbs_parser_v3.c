/**
 * @file uft_gbs_parser_v3.c
 * @brief GOD MODE GBS Parser v3 - Game Boy Sound
 * 
 * GB music rip format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define GBS_MAGIC               "GBS"
#define GBS_HEADER_SIZE         0x70

typedef struct {
    char signature[4];
    uint8_t version;
    uint8_t song_count;
    uint8_t first_song;
    uint16_t load_addr;
    uint16_t init_addr;
    uint16_t play_addr;
    uint16_t sp;
    char title[33];
    char author[33];
    char copyright[33];
    size_t source_size;
    bool valid;
} gbs_file_t;

static bool gbs_parse(const uint8_t* data, size_t size, gbs_file_t* gbs) {
    if (!data || !gbs || size < GBS_HEADER_SIZE) return false;
    memset(gbs, 0, sizeof(gbs_file_t));
    gbs->source_size = size;
    
    memcpy(gbs->signature, data, 3);
    gbs->signature[3] = '\0';
    
    if (memcmp(gbs->signature, GBS_MAGIC, 3) == 0) {
        gbs->version = data[3];
        gbs->song_count = data[4];
        gbs->first_song = data[5];
        gbs->load_addr = data[6] | (data[7] << 8);
        gbs->init_addr = data[8] | (data[9] << 8);
        gbs->play_addr = data[10] | (data[11] << 8);
        gbs->sp = data[12] | (data[13] << 8);
        memcpy(gbs->title, data + 0x10, 32); gbs->title[32] = '\0';
        memcpy(gbs->author, data + 0x30, 32); gbs->author[32] = '\0';
        memcpy(gbs->copyright, data + 0x50, 32); gbs->copyright[32] = '\0';
        gbs->valid = true;
    }
    return true;
}

#ifdef GBS_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== GBS Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t gbs[GBS_HEADER_SIZE] = {'G', 'B', 'S', 1, 10};
    gbs_file_t file;
    assert(gbs_parse(gbs, sizeof(gbs), &file) && file.song_count == 10);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
