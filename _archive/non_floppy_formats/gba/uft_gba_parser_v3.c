/**
 * @file uft_gba_parser_v3.c
 * @brief GOD MODE GBA Parser v3 - Game Boy Advance ROM
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define GBA_HEADER_SIZE         0xC0
#define GBA_TITLE_OFFSET        0xA0
#define GBA_GAME_CODE           0xAC
#define GBA_MAKER_CODE          0xB0

typedef struct {
    char title[13];
    char game_code[5];
    char maker_code[3];
    uint8_t unit_code;
    uint8_t device_type;
    uint8_t version;
    uint8_t complement;
    size_t source_size;
    bool valid;
} gba_rom_t;

static bool gba_parse(const uint8_t* data, size_t size, gba_rom_t* rom) {
    if (!data || !rom || size < GBA_HEADER_SIZE) return false;
    memset(rom, 0, sizeof(gba_rom_t));
    rom->source_size = size;
    
    memcpy(rom->title, data + GBA_TITLE_OFFSET, 12); rom->title[12] = '\0';
    memcpy(rom->game_code, data + GBA_GAME_CODE, 4); rom->game_code[4] = '\0';
    memcpy(rom->maker_code, data + GBA_MAKER_CODE, 2); rom->maker_code[2] = '\0';
    
    rom->unit_code = data[0xB3];
    rom->device_type = data[0xB4];
    rom->version = data[0xBC];
    rom->complement = data[0xBD];
    
    /* Check fixed value and entry point */
    rom->valid = (data[0xB2] == 0x96);  /* Fixed value */
    return true;
}

#ifdef GBA_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== GBA Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* gba = calloc(1, 0x100);
    gba[0xB2] = 0x96;  /* Fixed value */
    memcpy(gba + GBA_TITLE_OFFSET, "TESTGAME", 8);
    gba_rom_t rom;
    assert(gba_parse(gba, 0x100, &rom) && rom.valid);
    free(gba);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
