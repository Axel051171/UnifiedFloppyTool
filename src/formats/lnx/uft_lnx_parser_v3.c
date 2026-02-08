/**
 * @file uft_lnx_parser_v3.c
 * @brief GOD MODE LNX Parser v3 - Atari Lynx ROM
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define LNX_HEADER_SIZE         64
#define LNX_MAGIC               "LYNX"

typedef struct {
    char magic[5];
    uint16_t page_size_bank0;
    uint16_t page_size_bank1;
    uint16_t version;
    char title[33];
    char manufacturer[17];
    uint8_t rotation;
    size_t source_size;
    bool valid;
} lnx_rom_t;

static uint16_t lnx_read_le16(const uint8_t* p) { return p[0] | (p[1] << 8); }

static bool lnx_parse(const uint8_t* data, size_t size, lnx_rom_t* rom) {
    if (!data || !rom || size < LNX_HEADER_SIZE) return false;
    memset(rom, 0, sizeof(lnx_rom_t));
    rom->source_size = size;
    
    memcpy(rom->magic, data, 4); rom->magic[4] = '\0';
    rom->valid = (memcmp(rom->magic, LNX_MAGIC, 4) == 0);
    
    if (rom->valid) {
        rom->page_size_bank0 = lnx_read_le16(data + 4);
        rom->page_size_bank1 = lnx_read_le16(data + 6);
        rom->version = lnx_read_le16(data + 8);
        memcpy(rom->title, data + 10, 32); rom->title[32] = '\0';
        memcpy(rom->manufacturer, data + 42, 16); rom->manufacturer[16] = '\0';
        rom->rotation = data[58];
    }
    return true;
}

#ifdef LNX_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Atari Lynx Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t lnx[128] = {0};
    memcpy(lnx, "LYNX", 4);
    memcpy(lnx + 10, "TEST GAME", 9);
    lnx_rom_t rom;
    assert(lnx_parse(lnx, sizeof(lnx), &rom) && rom.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
