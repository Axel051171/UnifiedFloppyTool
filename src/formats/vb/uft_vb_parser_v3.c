/**
 * @file uft_vb_parser_v3.c
 * @brief GOD MODE VB Parser v3 - Nintendo Virtual Boy ROM
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define VB_HEADER_OFFSET        0xFFFFFDE0  /* From ROM end */
#define VB_MIN_SIZE             0x80000

typedef struct {
    char title[21];
    char maker_code[3];
    char game_code[5];
    uint8_t version;
    size_t source_size;
    bool valid;
} vb_rom_t;

static bool vb_parse(const uint8_t* data, size_t size, vb_rom_t* rom) {
    if (!data || !rom || size < VB_MIN_SIZE) return false;
    memset(rom, 0, sizeof(vb_rom_t));
    rom->source_size = size;
    
    /* Header at end of ROM - 0x220 */
    const uint8_t* h = data + size - 0x220;
    memcpy(rom->title, h, 20); rom->title[20] = '\0';
    memcpy(rom->maker_code, h + 0x19, 2); rom->maker_code[2] = '\0';
    memcpy(rom->game_code, h + 0x1B, 4); rom->game_code[4] = '\0';
    rom->version = h[0x1F];
    
    rom->valid = true;
    return true;
}

#ifdef VB_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Virtual Boy Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* vb = calloc(1, VB_MIN_SIZE);
    memcpy(vb + VB_MIN_SIZE - 0x220, "TEST GAME VB        ", 20);
    vb_rom_t rom;
    assert(vb_parse(vb, VB_MIN_SIZE, &rom) && rom.valid);
    free(vb);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
