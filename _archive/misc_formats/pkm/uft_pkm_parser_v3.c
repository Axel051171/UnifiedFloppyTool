/**
 * @file uft_pkm_parser_v3.c
 * @brief GOD MODE PKM Parser v3 - Pokémon Mini
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PKM_MIN_SIZE            0x10000
#define PKM_HEADER_OFFSET       0x21AC
#define PKM_MAGIC               "MN"

typedef struct {
    char game_id[5];
    char title[13];
    uint32_t rom_size;
    size_t source_size;
    bool valid;
} pkm_rom_t;

static bool pkm_parse(const uint8_t* data, size_t size, pkm_rom_t* rom) {
    if (!data || !rom || size < PKM_MIN_SIZE) return false;
    memset(rom, 0, sizeof(pkm_rom_t));
    rom->source_size = size;
    rom->rom_size = size;
    
    /* Check header at offset 0x21AC */
    if (size > PKM_HEADER_OFFSET + 20) {
        memcpy(rom->game_id, data + PKM_HEADER_OFFSET, 4);
        rom->game_id[4] = '\0';
        memcpy(rom->title, data + PKM_HEADER_OFFSET + 4, 12);
        rom->title[12] = '\0';
    }
    
    rom->valid = (memcmp(data + PKM_HEADER_OFFSET, PKM_MAGIC, 2) == 0);
    return true;
}

#ifdef PKM_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Pokémon Mini Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* pkm = calloc(1, 0x20000);
    memcpy(pkm + PKM_HEADER_OFFSET, "MNTEST", 6);
    pkm_rom_t rom;
    assert(pkm_parse(pkm, 0x20000, &rom) && rom.valid);
    free(pkm);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
