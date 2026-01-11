/**
 * @file uft_a78_parser_v3.c
 * @brief GOD MODE A78 Parser v3 - Atari 7800 ROM
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define A78_HEADER_SIZE         128
#define A78_SIGNATURE           "ATARI7800"

typedef struct {
    char title[33];
    uint32_t rom_size;
    uint8_t cart_type;
    uint8_t controller1;
    uint8_t controller2;
    bool has_header;
    size_t source_size;
    bool valid;
} a78_rom_t;

static bool a78_parse(const uint8_t* data, size_t size, a78_rom_t* rom) {
    if (!data || !rom || size < A78_HEADER_SIZE) return false;
    memset(rom, 0, sizeof(a78_rom_t));
    rom->source_size = size;
    
    /* Check for A78 header */
    if (data[0] == 1 && memcmp(data + 1, A78_SIGNATURE, 9) == 0) {
        rom->has_header = true;
        memcpy(rom->title, data + 17, 32); rom->title[32] = '\0';
        rom->rom_size = (data[49] << 24) | (data[50] << 16) | (data[51] << 8) | data[52];
        rom->cart_type = data[53];
        rom->controller1 = data[54];
        rom->controller2 = data[55];
    } else {
        rom->has_header = false;
        rom->rom_size = size;
    }
    
    rom->valid = true;
    return true;
}

#ifdef A78_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Atari 7800 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t a78[256] = {0};
    a78[0] = 1;
    memcpy(a78 + 1, "ATARI7800", 9);
    memcpy(a78 + 17, "TEST GAME", 9);
    a78_rom_t rom;
    assert(a78_parse(a78, sizeof(a78), &rom) && rom.has_header);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
