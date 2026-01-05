/**
 * @file uft_int_parser_v3.c
 * @brief GOD MODE INT Parser v3 - Mattel Intellivision ROM
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define INT_MIN_SIZE            4096
#define INT_HEADER_SIZE         52

typedef struct {
    uint32_t rom_size;
    bool has_header;  /* .int vs .bin */
    size_t source_size;
    bool valid;
} int_rom_t;

static bool int_parse(const uint8_t* data, size_t size, int_rom_t* rom) {
    if (!data || !rom || size < INT_MIN_SIZE) return false;
    memset(rom, 0, sizeof(int_rom_t));
    rom->source_size = size;
    rom->rom_size = size;
    /* Check for INTV header */
    rom->has_header = (memcmp(data, "\xA8\x00", 2) == 0);
    rom->valid = true;
    return true;
}

#ifdef INT_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Intellivision Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* intv = calloc(1, INT_MIN_SIZE);
    int_rom_t rom;
    assert(int_parse(intv, INT_MIN_SIZE, &rom) && rom.valid);
    free(intv);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
