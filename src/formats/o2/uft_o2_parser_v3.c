/**
 * @file uft_o2_parser_v3.c
 * @brief GOD MODE O2 Parser v3 - Odyssey2/Videopac ROM
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define O2_MIN_SIZE             2048
#define O2_MAX_SIZE             16384

typedef struct {
    uint32_t rom_size;
    size_t source_size;
    bool valid;
} o2_rom_t;

static bool o2_parse(const uint8_t* data, size_t size, o2_rom_t* rom) {
    if (!data || !rom || size < O2_MIN_SIZE) return false;
    memset(rom, 0, sizeof(o2_rom_t));
    rom->source_size = size;
    rom->rom_size = size;
    rom->valid = (size >= O2_MIN_SIZE && size <= O2_MAX_SIZE);
    return true;
}

#ifdef O2_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Odyssey2 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* o2 = calloc(1, O2_MIN_SIZE);
    o2_rom_t rom;
    assert(o2_parse(o2, O2_MIN_SIZE, &rom) && rom.valid);
    free(o2);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
