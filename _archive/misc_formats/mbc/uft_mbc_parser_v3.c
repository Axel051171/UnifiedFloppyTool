/**
 * @file uft_mbc_parser_v3.c
 * @brief GOD MODE MBC Parser v3 - Game Boy Memory Bank Controller Detection
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t cartridge_type;
    uint8_t mbc_type;      /* 0=ROM, 1=MBC1, 2=MBC2, 3=MBC3, 5=MBC5, 6=MBC6, 7=MBC7 */
    bool has_ram;
    bool has_battery;
    bool has_timer;
    bool has_rumble;
    uint8_t rom_banks;
    uint8_t ram_banks;
    size_t source_size;
    bool valid;
} mbc_file_t;

static bool mbc_parse(const uint8_t* data, size_t size, mbc_file_t* mbc) {
    if (!data || !mbc || size < 0x150) return false;
    memset(mbc, 0, sizeof(mbc_file_t));
    mbc->source_size = size;
    
    mbc->cartridge_type = data[0x147];
    
    /* Detect MBC type */
    switch (mbc->cartridge_type) {
        case 0x00: mbc->mbc_type = 0; break;
        case 0x01: case 0x02: case 0x03: mbc->mbc_type = 1; break;
        case 0x05: case 0x06: mbc->mbc_type = 2; break;
        case 0x0F: case 0x10: case 0x11: case 0x12: case 0x13: mbc->mbc_type = 3; break;
        case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: mbc->mbc_type = 5; break;
        default: mbc->mbc_type = 0;
    }
    
    mbc->has_ram = (mbc->cartridge_type == 0x02 || mbc->cartridge_type == 0x03);
    mbc->has_battery = (mbc->cartridge_type == 0x03 || mbc->cartridge_type == 0x06);
    mbc->has_timer = (mbc->cartridge_type == 0x0F || mbc->cartridge_type == 0x10);
    
    mbc->rom_banks = data[0x148];
    mbc->ram_banks = data[0x149];
    mbc->valid = true;
    return true;
}

#ifdef MBC_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== MBC Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* rom = calloc(1, 0x150);
    rom[0x147] = 0x13;  /* MBC3+RAM+BATTERY */
    mbc_file_t file;
    assert(mbc_parse(rom, 0x150, &file) && file.mbc_type == 3);
    free(rom);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
