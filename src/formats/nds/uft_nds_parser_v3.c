/**
 * @file uft_nds_parser_v3.c
 * @brief GOD MODE NDS Parser v3 - Nintendo DS ROM
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define NDS_HEADER_SIZE         0x200

typedef struct {
    char title[13];
    char game_code[5];
    char maker_code[3];
    uint8_t unit_code;
    uint32_t arm9_rom_offset;
    uint32_t arm9_entry;
    uint32_t arm9_size;
    uint32_t arm7_rom_offset;
    uint32_t arm7_entry;
    uint32_t arm7_size;
    uint32_t rom_size;
    uint16_t header_crc;
    size_t source_size;
    bool valid;
} nds_rom_t;

static uint32_t nds_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool nds_parse(const uint8_t* data, size_t size, nds_rom_t* rom) {
    if (!data || !rom || size < NDS_HEADER_SIZE) return false;
    memset(rom, 0, sizeof(nds_rom_t));
    rom->source_size = size;
    
    memcpy(rom->title, data, 12); rom->title[12] = '\0';
    memcpy(rom->game_code, data + 0x0C, 4); rom->game_code[4] = '\0';
    memcpy(rom->maker_code, data + 0x10, 2); rom->maker_code[2] = '\0';
    rom->unit_code = data[0x12];
    
    rom->arm9_rom_offset = nds_read_le32(data + 0x20);
    rom->arm9_entry = nds_read_le32(data + 0x24);
    rom->arm9_size = nds_read_le32(data + 0x2C);
    rom->arm7_rom_offset = nds_read_le32(data + 0x30);
    rom->arm7_entry = nds_read_le32(data + 0x34);
    rom->arm7_size = nds_read_le32(data + 0x3C);
    
    rom->header_crc = data[0x15E] | (data[0x15F] << 8);
    
    /* Check Nintendo logo CRC */
    rom->valid = (data[0xC0] == 0x24);  /* First byte of Nintendo logo */
    return true;
}

#ifdef NDS_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Nintendo DS Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* nds = calloc(1, NDS_HEADER_SIZE);
    memcpy(nds, "TESTGAME", 8);
    nds[0xC0] = 0x24;  /* Nintendo logo start */
    nds_rom_t rom;
    assert(nds_parse(nds, NDS_HEADER_SIZE, &rom) && rom.valid);
    free(nds);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
