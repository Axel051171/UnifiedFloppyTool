/**
 * @file uft_32x_parser_v3.c
 * @brief GOD MODE 32X Parser v3 - Sega 32X ROM
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define X32_HEADER_OFFSET       0x100

typedef struct {
    char console[17];
    char copyright[17];
    char title_domestic[49];
    char title_overseas[49];
    char serial[15];
    uint32_t rom_start;
    uint32_t rom_end;
    char region[4];
    size_t source_size;
    bool valid;
} x32_rom_t;

static bool x32_parse(const uint8_t* data, size_t size, x32_rom_t* rom) {
    if (!data || !rom || size < 0x200) return false;
    memset(rom, 0, sizeof(x32_rom_t));
    rom->source_size = size;
    
    const uint8_t* h = data + X32_HEADER_OFFSET;
    memcpy(rom->console, h, 16); rom->console[16] = '\0';
    memcpy(rom->title_domestic, h + 0x20, 48); rom->title_domestic[48] = '\0';
    memcpy(rom->title_overseas, h + 0x50, 48); rom->title_overseas[48] = '\0';
    
    /* 32X ROMs have "SEGA 32X" in console string */
    rom->valid = (strstr(rom->console, "32X") != NULL);
    return true;
}

#ifdef X32_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Sega 32X Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* x32 = calloc(1, 0x200);
    memcpy(x32 + X32_HEADER_OFFSET, "SEGA 32X        ", 16);
    x32_rom_t rom;
    assert(x32_parse(x32, 0x200, &rom) && rom.valid);
    free(x32);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
