/**
 * @file uft_tgc_parser_v3.c
 * @brief GOD MODE TGC Parser v3 - Tiger Game.com
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define TGC_MIN_SIZE            0x20000
#define TGC_HEADER_SIZE         64

typedef struct {
    char game_name[17];
    uint32_t rom_size;
    size_t source_size;
    bool valid;
} tgc_rom_t;

static bool tgc_parse(const uint8_t* data, size_t size, tgc_rom_t* rom) {
    if (!data || !rom || size < TGC_MIN_SIZE) return false;
    memset(rom, 0, sizeof(tgc_rom_t));
    rom->source_size = size;
    rom->rom_size = size;
    memcpy(rom->game_name, data, 16); rom->game_name[16] = '\0';
    rom->valid = true;
    return true;
}

#ifdef TGC_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Tiger Game.com Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* tgc = calloc(1, TGC_MIN_SIZE);
    memcpy(tgc, "TESTGAME", 8);
    tgc_rom_t rom;
    assert(tgc_parse(tgc, TGC_MIN_SIZE, &rom) && rom.valid);
    free(tgc);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
