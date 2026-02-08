/**
 * @file uft_neo_parser_v3.c
 * @brief GOD MODE NEO Parser v3 - SNK Neo Geo AES/MVS ROM
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define NEO_HEADER_SIZE         0x1000
#define NEO_MAGIC               "NEO-GEO"

typedef struct {
    char signature[8];
    uint32_t p_rom_size;
    uint32_t s_rom_size;
    uint32_t m_rom_size;
    uint32_t v_rom_size;
    uint32_t c_rom_size;
    char ngh_id[5];
    size_t source_size;
    bool valid;
} neo_rom_t;

static uint32_t neo_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool neo_parse(const uint8_t* data, size_t size, neo_rom_t* rom) {
    if (!data || !rom || size < NEO_HEADER_SIZE) return false;
    memset(rom, 0, sizeof(neo_rom_t));
    rom->source_size = size;
    
    memcpy(rom->signature, data, 7); rom->signature[7] = '\0';
    rom->p_rom_size = neo_read_le32(data + 0x10);
    rom->s_rom_size = neo_read_le32(data + 0x14);
    rom->m_rom_size = neo_read_le32(data + 0x18);
    rom->v_rom_size = neo_read_le32(data + 0x1C);
    rom->c_rom_size = neo_read_le32(data + 0x20);
    
    rom->valid = (memcmp(rom->signature, NEO_MAGIC, 7) == 0);
    return true;
}

#ifdef NEO_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Neo Geo Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* neo = calloc(1, NEO_HEADER_SIZE);
    memcpy(neo, "NEO-GEO", 7);
    neo_rom_t rom;
    assert(neo_parse(neo, NEO_HEADER_SIZE, &rom) && rom.valid);
    free(neo);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
