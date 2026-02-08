/**
 * @file uft_wsc_parser_v3.c
 * @brief GOD MODE WSC Parser v3 - Bandai WonderSwan/Color ROM
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t developer_id;
    uint8_t color_mode;      /* 0=mono, 1=color */
    uint8_t game_id;
    uint8_t version;
    uint8_t rom_size_code;
    uint8_t save_type;
    uint8_t flags;
    uint16_t checksum;
    size_t source_size;
    bool valid;
} wsc_rom_t;

static bool wsc_parse(const uint8_t* data, size_t size, wsc_rom_t* rom) {
    if (!data || !rom || size < 0x10000) return false;
    memset(rom, 0, sizeof(wsc_rom_t));
    rom->source_size = size;
    
    /* Header at end of ROM */
    const uint8_t* h = data + size - 10;
    rom->developer_id = h[0];
    rom->color_mode = h[1];
    rom->game_id = h[2];
    rom->version = h[3];
    rom->rom_size_code = h[4];
    rom->save_type = h[5];
    rom->flags = h[6];
    rom->checksum = h[8] | (h[9] << 8);
    
    rom->valid = true;
    return true;
}

#ifdef WSC_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== WonderSwan Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* wsc = calloc(1, 0x10000);
    wsc_rom_t rom;
    assert(wsc_parse(wsc, 0x10000, &rom) && rom.valid);
    free(wsc);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
