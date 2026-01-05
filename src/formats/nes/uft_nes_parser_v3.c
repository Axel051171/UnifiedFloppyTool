/**
 * @file uft_nes_parser_v3.c
 * @brief GOD MODE NES Parser v3 - Nintendo Entertainment System ROM
 * 
 * iNES/NES 2.0 ROM Format:
 * - 16-byte Header
 * - PRG-ROM, CHR-ROM
 * - Mapper Info
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define NES_HEADER_SIZE         16
#define NES_PRG_UNIT            16384
#define NES_CHR_UNIT            8192

static const uint8_t NES_MAGIC[] = {0x4E, 0x45, 0x53, 0x1A};  /* "NES\x1A" */

typedef struct {
    uint8_t prg_rom_size;    /* In 16KB units */
    uint8_t chr_rom_size;    /* In 8KB units */
    uint8_t mapper;
    uint8_t mirroring;       /* 0=horizontal, 1=vertical */
    bool has_battery;
    bool has_trainer;
    bool is_nes20;
    uint32_t prg_bytes;
    uint32_t chr_bytes;
    size_t source_size;
    bool valid;
} nes_rom_t;

static bool nes_parse(const uint8_t* data, size_t size, nes_rom_t* rom) {
    if (!data || !rom || size < NES_HEADER_SIZE) return false;
    memset(rom, 0, sizeof(nes_rom_t));
    rom->source_size = size;
    
    /* Check magic */
    if (memcmp(data, NES_MAGIC, 4) != 0) return false;
    
    rom->prg_rom_size = data[4];
    rom->chr_rom_size = data[5];
    rom->mirroring = data[6] & 0x01;
    rom->has_battery = (data[6] & 0x02) != 0;
    rom->has_trainer = (data[6] & 0x04) != 0;
    rom->mapper = ((data[6] >> 4) & 0x0F) | (data[7] & 0xF0);
    rom->is_nes20 = ((data[7] & 0x0C) == 0x08);
    
    rom->prg_bytes = rom->prg_rom_size * NES_PRG_UNIT;
    rom->chr_bytes = rom->chr_rom_size * NES_CHR_UNIT;
    
    rom->valid = true;
    return true;
}

#ifdef NES_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== NES Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t nes[32] = {0x4E, 0x45, 0x53, 0x1A, 2, 1, 0x10, 0x00};
    nes_rom_t rom;
    assert(nes_parse(nes, sizeof(nes), &rom) && rom.prg_rom_size == 2);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
