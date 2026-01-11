/**
 * @file uft_snes_parser_v3.c
 * @brief GOD MODE SNES Parser v3 - Super Nintendo ROM
 * 
 * SNES ROM Format:
 * - LoROM/HiROM/ExHiROM
 * - Optional 512-byte SMC header
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define SNES_LOROM_HEADER       0x7FC0
#define SNES_HIROM_HEADER       0xFFC0
#define SNES_SMC_HEADER         512

typedef enum {
    SNES_MODE_LOROM = 0x20,
    SNES_MODE_HIROM = 0x21,
    SNES_MODE_EXHIROM = 0x25
} snes_mode_t;

typedef struct {
    char title[22];
    snes_mode_t mode;
    uint8_t rom_size;        /* In 1KB << n */
    uint8_t ram_size;
    uint8_t country;
    uint8_t developer;
    uint8_t version;
    uint16_t checksum;
    uint16_t checksum_comp;
    bool has_smc_header;
    size_t source_size;
    bool valid;
} snes_rom_t;

static uint16_t snes_read_le16(const uint8_t* p) { return p[0] | (p[1] << 8); }

static bool snes_parse(const uint8_t* data, size_t size, snes_rom_t* rom) {
    if (!data || !rom || size < 32768) return false;
    memset(rom, 0, sizeof(snes_rom_t));
    rom->source_size = size;
    
    /* Check for SMC header */
    rom->has_smc_header = ((size & 0x7FFF) == SNES_SMC_HEADER);
    size_t offset = rom->has_smc_header ? SNES_SMC_HEADER : 0;
    
    /* Try LoROM first, then HiROM */
    const uint8_t* header = NULL;
    if (offset + SNES_HIROM_HEADER + 32 <= size) {
        header = data + offset + SNES_HIROM_HEADER;
        if ((header[0x15] & 0x01) == 0x01) {
            rom->mode = SNES_MODE_HIROM;
        } else {
            header = data + offset + SNES_LOROM_HEADER;
            rom->mode = SNES_MODE_LOROM;
        }
    } else if (offset + SNES_LOROM_HEADER + 32 <= size) {
        header = data + offset + SNES_LOROM_HEADER;
        rom->mode = SNES_MODE_LOROM;
    } else {
        return false;
    }
    
    memcpy(rom->title, header, 21);
    rom->title[21] = '\0';
    rom->rom_size = header[0x17];
    rom->ram_size = header[0x18];
    rom->country = header[0x19];
    rom->developer = header[0x1A];
    rom->version = header[0x1B];
    rom->checksum_comp = snes_read_le16(header + 0x1C);
    rom->checksum = snes_read_le16(header + 0x1E);
    
    rom->valid = true;
    return true;
}

#ifdef SNES_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== SNES Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* snes = calloc(1, 65536);
    memcpy(snes + SNES_LOROM_HEADER, "TEST GAME            ", 21);
    snes_rom_t rom;
    assert(snes_parse(snes, 65536, &rom) && rom.valid);
    free(snes);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
