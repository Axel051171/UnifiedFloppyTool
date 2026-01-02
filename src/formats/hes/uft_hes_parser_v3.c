/**
 * @file uft_hes_parser_v3.c
 * @brief GOD MODE HES Parser v3 - PC Engine/TG16 Sound
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define HES_MAGIC               "HESM"

typedef struct {
    char signature[5];
    uint8_t version;
    uint8_t starting_song;
    uint8_t song_count;
    uint16_t load_address;
    uint16_t init_address;
    uint16_t first_page;
    uint8_t total_pages;
    size_t source_size;
    bool valid;
} hes_file_t;

static bool hes_parse(const uint8_t* data, size_t size, hes_file_t* hes) {
    if (!data || !hes || size < 16) return false;
    memset(hes, 0, sizeof(hes_file_t));
    hes->source_size = size;
    
    if (memcmp(data, HES_MAGIC, 4) == 0) {
        memcpy(hes->signature, data, 4);
        hes->signature[4] = '\0';
        hes->version = data[4];
        hes->starting_song = data[5];
        hes->load_address = data[6] | (data[7] << 8);
        hes->init_address = data[8] | (data[9] << 8);
        hes->first_page = data[10];
        hes->total_pages = data[11];
        hes->valid = true;
    }
    return true;
}

#ifdef HES_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== HES Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t hes[32] = {'H', 'E', 'S', 'M', 1, 0, 0x00, 0x40};
    hes_file_t file;
    assert(hes_parse(hes, sizeof(hes), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
