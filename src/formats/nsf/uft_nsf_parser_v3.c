/**
 * @file uft_nsf_parser_v3.c
 * @brief GOD MODE NSF Parser v3 - NES Sound Format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define NSF_MAGIC               "NESM\x1A"
#define NSF_HEADER_SIZE         0x80

typedef struct {
    char signature[6];
    uint8_t version;
    uint8_t song_count;
    uint8_t starting_song;
    uint16_t load_addr;
    uint16_t init_addr;
    uint16_t play_addr;
    char title[33];
    char artist[33];
    char copyright[33];
    uint16_t ntsc_speed;
    uint8_t bankswitch[8];
    uint16_t pal_speed;
    uint8_t pal_ntsc_bits;
    uint8_t extra_sound;
    size_t source_size;
    bool valid;
} nsf_file_t;

static bool nsf_parse(const uint8_t* data, size_t size, nsf_file_t* nsf) {
    if (!data || !nsf || size < NSF_HEADER_SIZE) return false;
    memset(nsf, 0, sizeof(nsf_file_t));
    nsf->source_size = size;
    
    memcpy(nsf->signature, data, 5);
    nsf->signature[5] = '\0';
    
    if (memcmp(data, NSF_MAGIC, 5) == 0) {
        nsf->version = data[5];
        nsf->song_count = data[6];
        nsf->starting_song = data[7];
        nsf->load_addr = data[8] | (data[9] << 8);
        nsf->init_addr = data[10] | (data[11] << 8);
        nsf->play_addr = data[12] | (data[13] << 8);
        memcpy(nsf->title, data + 0x0E, 32); nsf->title[32] = '\0';
        memcpy(nsf->artist, data + 0x2E, 32); nsf->artist[32] = '\0';
        memcpy(nsf->copyright, data + 0x4E, 32); nsf->copyright[32] = '\0';
        nsf->extra_sound = data[0x7B];
        nsf->valid = true;
    }
    return true;
}

#ifdef NSF_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== NSF Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t nsf[NSF_HEADER_SIZE] = {'N', 'E', 'S', 'M', 0x1A, 1, 15};
    nsf_file_t file;
    assert(nsf_parse(nsf, sizeof(nsf), &file) && file.song_count == 15);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
