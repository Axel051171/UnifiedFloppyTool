/**
 * @file uft_gb_parser_v3.c
 * @brief GOD MODE GB Parser v3 - Game Boy/Game Boy Color ROM
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define GB_HEADER_OFFSET        0x100
#define GB_TITLE_OFFSET         0x134
#define GB_CGB_FLAG             0x143
#define GB_CART_TYPE            0x147
#define GB_ROM_SIZE             0x148
#define GB_RAM_SIZE             0x149

typedef struct {
    char title[17];
    uint8_t cgb_flag;
    uint8_t cart_type;
    uint8_t rom_size_code;
    uint8_t ram_size_code;
    uint8_t header_checksum;
    uint16_t global_checksum;
    bool is_cgb;
    bool is_sgb;
    size_t source_size;
    bool valid;
} gb_rom_t;

static bool gb_parse(const uint8_t* data, size_t size, gb_rom_t* rom) {
    if (!data || !rom || size < 0x150) return false;
    memset(rom, 0, sizeof(gb_rom_t));
    rom->source_size = size;
    
    memcpy(rom->title, data + GB_TITLE_OFFSET, 16); rom->title[16] = '\0';
    rom->cgb_flag = data[GB_CGB_FLAG];
    rom->cart_type = data[GB_CART_TYPE];
    rom->rom_size_code = data[GB_ROM_SIZE];
    rom->ram_size_code = data[GB_RAM_SIZE];
    rom->header_checksum = data[0x14D];
    rom->global_checksum = (data[0x14E] << 8) | data[0x14F];
    
    rom->is_cgb = (rom->cgb_flag == 0x80 || rom->cgb_flag == 0xC0);
    rom->is_sgb = (data[0x146] == 0x03);
    
    /* Verify Nintendo logo (simplified check) */
    rom->valid = (data[0x104] == 0xCE && data[0x105] == 0xED);
    return true;
}

#ifdef GB_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Game Boy Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* gb = calloc(1, 0x8000);
    gb[0x104] = 0xCE; gb[0x105] = 0xED;  /* Nintendo logo start */
    memcpy(gb + GB_TITLE_OFFSET, "TESTGAME", 8);
    gb_rom_t rom;
    assert(gb_parse(gb, 0x8000, &rom) && rom.valid);
    free(gb);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
