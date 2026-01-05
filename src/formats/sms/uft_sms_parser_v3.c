/**
 * @file uft_sms_parser_v3.c
 * @brief GOD MODE SMS Parser v3 - Sega Master System/Game Gear ROM
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define SMS_HEADER_OFFSET       0x7FF0
#define SMS_MAGIC               "TMR SEGA"

typedef struct {
    char signature[9];
    uint16_t checksum;
    uint32_t product_code;
    uint8_t version;
    uint8_t region;
    uint8_t rom_size_code;
    bool is_game_gear;
    size_t source_size;
    bool valid;
} sms_rom_t;

static bool sms_parse(const uint8_t* data, size_t size, sms_rom_t* rom) {
    if (!data || !rom || size < 0x8000) return false;
    memset(rom, 0, sizeof(sms_rom_t));
    rom->source_size = size;
    
    const uint8_t* h = data + SMS_HEADER_OFFSET;
    memcpy(rom->signature, h, 8); rom->signature[8] = '\0';
    
    rom->checksum = h[0x0A] | (h[0x0B] << 8);
    rom->product_code = h[0x0C] | (h[0x0D] << 8) | ((h[0x0E] & 0xF0) << 12);
    rom->version = h[0x0E] & 0x0F;
    rom->region = h[0x0F] >> 4;
    rom->rom_size_code = h[0x0F] & 0x0F;
    
    rom->is_game_gear = (rom->region >= 5 && rom->region <= 7);
    rom->valid = (memcmp(rom->signature, SMS_MAGIC, 8) == 0);
    return true;
}

#ifdef SMS_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== SMS Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* sms = calloc(1, 0x8000);
    memcpy(sms + SMS_HEADER_OFFSET, "TMR SEGA", 8);
    sms_rom_t rom;
    assert(sms_parse(sms, 0x8000, &rom) && rom.valid);
    free(sms);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
