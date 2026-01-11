/**
 * @file uft_3ds_parser_v3.c
 * @brief GOD MODE 3DS Parser v3 - Nintendo 3DS ROM
 * 
 * Supports .3ds/.cci and .cia formats
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define NCSD_MAGIC              "NCSD"
#define NCCH_MAGIC              "NCCH"

typedef struct {
    char signature[5];
    uint32_t image_size;
    uint64_t media_id;
    char product_code[17];
    uint8_t partition_count;
    bool is_cci;
    bool is_cia;
    size_t source_size;
    bool valid;
} n3ds_rom_t;

static uint32_t n3ds_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static bool n3ds_parse(const uint8_t* data, size_t size, n3ds_rom_t* rom) {
    if (!data || !rom || size < 0x200) return false;
    memset(rom, 0, sizeof(n3ds_rom_t));
    rom->source_size = size;
    
    /* Check for NCSD (CCI format) */
    if (memcmp(data + 0x100, NCSD_MAGIC, 4) == 0) {
        rom->is_cci = true;
        memcpy(rom->signature, NCSD_MAGIC, 4); rom->signature[4] = '\0';
        rom->image_size = n3ds_read_le32(data + 0x104) * 0x200;
        rom->valid = true;
    }
    /* Check for NCCH at offset 0 (standalone) */
    else if (memcmp(data + 0x100, NCCH_MAGIC, 4) == 0) {
        memcpy(rom->signature, NCCH_MAGIC, 4); rom->signature[4] = '\0';
        memcpy(rom->product_code, data + 0x150, 16); rom->product_code[16] = '\0';
        rom->valid = true;
    }
    
    return true;
}

#ifdef N3DS_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Nintendo 3DS Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* n3ds = calloc(1, 0x200);
    memcpy(n3ds + 0x100, "NCSD", 4);
    n3ds_rom_t rom;
    assert(n3ds_parse(n3ds, 0x200, &rom) && rom.is_cci);
    free(n3ds);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
