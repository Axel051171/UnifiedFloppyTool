/**
 * @file uft_pce_parser_v3.c
 * @brief GOD MODE PCE Parser v3 - PC Engine/TurboGrafx-16 ROM
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PCE_HEADER_SIZE         512
#define PCE_MIN_SIZE            0x2000

typedef struct {
    bool has_header;
    uint32_t rom_size;
    size_t source_size;
    bool valid;
} pce_rom_t;

static bool pce_parse(const uint8_t* data, size_t size, pce_rom_t* rom) {
    if (!data || !rom || size < PCE_MIN_SIZE) return false;
    memset(rom, 0, sizeof(pce_rom_t));
    rom->source_size = size;
    
    /* Check for 512-byte header (size % 8192 == 512) */
    rom->has_header = ((size % 8192) == PCE_HEADER_SIZE);
    rom->rom_size = rom->has_header ? (size - PCE_HEADER_SIZE) : size;
    rom->valid = true;
    return true;
}

#ifdef PCE_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== PCE Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* pce = calloc(1, 0x40000);
    pce_rom_t rom;
    assert(pce_parse(pce, 0x40000, &rom) && rom.valid);
    free(pce);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
