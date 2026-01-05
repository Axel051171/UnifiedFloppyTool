/**
 * @file uft_col_parser_v3.c
 * @brief GOD MODE COL Parser v3 - ColecoVision ROM
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define COL_MIN_SIZE            8192
#define COL_MAX_SIZE            32768

typedef struct {
    uint32_t rom_size;
    uint16_t start_address;
    size_t source_size;
    bool valid;
} col_rom_t;

static bool col_parse(const uint8_t* data, size_t size, col_rom_t* rom) {
    if (!data || !rom || size < COL_MIN_SIZE) return false;
    memset(rom, 0, sizeof(col_rom_t));
    rom->source_size = size;
    rom->rom_size = size;
    /* First two bytes are usually AA 55 or 55 AA */
    rom->valid = ((data[0] == 0xAA && data[1] == 0x55) || 
                  (data[0] == 0x55 && data[1] == 0xAA));
    rom->start_address = data[2] | (data[3] << 8);
    return true;
}

#ifdef COL_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== ColecoVision Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* col = calloc(1, COL_MIN_SIZE);
    col[0] = 0xAA; col[1] = 0x55;
    col_rom_t rom;
    assert(col_parse(col, COL_MIN_SIZE, &rom) && rom.valid);
    free(col);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
