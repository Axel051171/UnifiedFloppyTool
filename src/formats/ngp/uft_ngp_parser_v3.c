/**
 * @file uft_ngp_parser_v3.c
 * @brief GOD MODE NGP Parser v3 - Neo Geo Pocket/Color ROM
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define NGP_HEADER_OFFSET       0x00
#define NGP_COPYRIGHT           "COPYRIGHT BY SNK"

typedef struct {
    char copyright[17];
    uint32_t startup_addr;
    uint16_t game_id;
    uint8_t version;
    bool is_color;
    char title[13];
    size_t source_size;
    bool valid;
} ngp_rom_t;

static bool ngp_parse(const uint8_t* data, size_t size, ngp_rom_t* rom) {
    if (!data || !rom || size < 0x1000) return false;
    memset(rom, 0, sizeof(ngp_rom_t));
    rom->source_size = size;
    
    memcpy(rom->copyright, data, 16); rom->copyright[16] = '\0';
    rom->startup_addr = data[0x1C] | (data[0x1D] << 8) | ((uint32_t)data[0x1E] << 16) | ((uint32_t)data[0x1F] << 24);
    rom->game_id = data[0x20] | (data[0x21] << 8);
    rom->version = data[0x22];
    rom->is_color = (data[0x23] == 0x10);
    memcpy(rom->title, data + 0x24, 12); rom->title[12] = '\0';
    
    rom->valid = (memcmp(rom->copyright, NGP_COPYRIGHT, 16) == 0);
    return true;
}

#ifdef NGP_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Neo Geo Pocket Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* ngp = calloc(1, 0x1000);
    memcpy(ngp, "COPYRIGHT BY SNK", 16);
    ngp_rom_t rom;
    assert(ngp_parse(ngp, 0x1000, &rom) && rom.valid);
    free(ngp);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
