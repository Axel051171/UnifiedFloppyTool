/**
 * @file uft_gen_parser_v3.c
 * @brief GOD MODE GEN Parser v3 - Sega Genesis/Mega Drive ROM
 * 
 * Genesis/Mega Drive ROM:
 * - 512-byte header at 0x100
 * - Big-endian 68000
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define GEN_HEADER_OFFSET       0x100
#define GEN_MIN_SIZE            0x200

typedef struct {
    char console[17];        /* "SEGA MEGA DRIVE " or "SEGA GENESIS    " */
    char copyright[17];
    char title_domestic[49];
    char title_overseas[49];
    char serial[15];
    uint32_t rom_start;
    uint32_t rom_end;
    uint32_t ram_start;
    uint32_t ram_end;
    char region[4];
    size_t source_size;
    bool valid;
} gen_rom_t;

static uint32_t gen_read_be32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static bool gen_parse(const uint8_t* data, size_t size, gen_rom_t* rom) {
    if (!data || !rom || size < GEN_MIN_SIZE) return false;
    memset(rom, 0, sizeof(gen_rom_t));
    rom->source_size = size;
    
    const uint8_t* h = data + GEN_HEADER_OFFSET;
    
    memcpy(rom->console, h, 16); rom->console[16] = '\0';
    memcpy(rom->copyright, h + 0x10, 16); rom->copyright[16] = '\0';
    memcpy(rom->title_domestic, h + 0x20, 48); rom->title_domestic[48] = '\0';
    memcpy(rom->title_overseas, h + 0x50, 48); rom->title_overseas[48] = '\0';
    memcpy(rom->serial, h + 0x80, 14); rom->serial[14] = '\0';
    
    rom->rom_start = gen_read_be32(h + 0xA0);
    rom->rom_end = gen_read_be32(h + 0xA4);
    rom->ram_start = gen_read_be32(h + 0xA8);
    rom->ram_end = gen_read_be32(h + 0xAC);
    
    memcpy(rom->region, h + 0xF0, 3); rom->region[3] = '\0';
    
    /* Validate console string */
    rom->valid = (strncmp(rom->console, "SEGA", 4) == 0);
    return true;
}

#ifdef GEN_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Genesis Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* gen = calloc(1, 0x200);
    memcpy(gen + GEN_HEADER_OFFSET, "SEGA MEGA DRIVE ", 16);
    gen_rom_t rom;
    assert(gen_parse(gen, 0x200, &rom) && rom.valid);
    free(gen);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
