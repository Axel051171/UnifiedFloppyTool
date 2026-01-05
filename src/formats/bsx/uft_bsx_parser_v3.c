/**
 * @file uft_bsx_parser_v3.c
 * @brief GOD MODE BSX Parser v3 - Satellaview ROM
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
    char title[17];
    uint8_t block_allocation[4];
    uint16_t limited_starts;
    uint8_t month;
    uint8_t day;
    uint8_t rom_type;
    uint8_t soundlink;
    uint8_t execution_area;
    size_t source_size;
    bool valid;
} bsx_rom_t;

static bool bsx_parse(const uint8_t* data, size_t size, bsx_rom_t* bsx) {
    if (!data || !bsx || size < 0x8000) return false;
    memset(bsx, 0, sizeof(bsx_rom_t));
    bsx->source_size = size;
    
    /* BS-X header at 0x7FB0 or 0xFFB0 */
    size_t offsets[] = {0x7FB0, 0xFFB0, 0x40FFB0};
    for (int i = 0; i < 3 && !bsx->valid; i++) {
        if (offsets[i] + 32 > size) continue;
        const uint8_t* hdr = data + offsets[i];
        
        /* Check for valid BS-X markers */
        if (hdr[0x0D] == 0x00 || hdr[0x0D] == 0x10) {
            memcpy(bsx->title, hdr, 16);
            bsx->title[16] = '\0';
            bsx->rom_type = hdr[0x0D];
            bsx->month = hdr[0x06];
            bsx->day = hdr[0x07];
            bsx->valid = true;
        }
    }
    return true;
}

#ifdef BSX_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Satellaview Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* bsx = calloc(1, 0x8000);
    bsx[0x7FBD] = 0x10;  /* BS-X marker */
    bsx_rom_t rom;
    assert(bsx_parse(bsx, 0x8000, &rom) && rom.valid);
    free(bsx);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
