/**
 * @file uft_cgb_parser_v3.c
 * @brief GOD MODE CGB Parser v3 - Game Boy Color Header
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
    char manufacturer[5];
    uint8_t cgb_flag;
    char new_licensee[3];
    uint8_t sgb_flag;
    uint8_t destination;
    uint8_t rom_version;
    uint8_t header_checksum;
    uint16_t global_checksum;
    bool is_cgb_only;
    bool is_cgb_enhanced;
    bool is_sgb_enhanced;
    size_t source_size;
    bool valid;
} cgb_file_t;

static bool cgb_parse(const uint8_t* data, size_t size, cgb_file_t* cgb) {
    if (!data || !cgb || size < 0x150) return false;
    memset(cgb, 0, sizeof(cgb_file_t));
    cgb->source_size = size;
    
    memcpy(cgb->title, data + 0x134, 16);
    cgb->title[16] = '\0';
    
    cgb->cgb_flag = data[0x143];
    cgb->sgb_flag = data[0x146];
    cgb->destination = data[0x14A];
    cgb->rom_version = data[0x14C];
    cgb->header_checksum = data[0x14D];
    cgb->global_checksum = (data[0x14E] << 8) | data[0x14F];
    
    cgb->is_cgb_only = (cgb->cgb_flag == 0xC0);
    cgb->is_cgb_enhanced = (cgb->cgb_flag == 0x80);
    cgb->is_sgb_enhanced = (cgb->sgb_flag == 0x03);
    
    cgb->valid = true;
    return true;
}

#ifdef CGB_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== CGB Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* rom = calloc(1, 0x150);
    rom[0x143] = 0x80;  /* CGB enhanced */
    rom[0x146] = 0x03;  /* SGB enhanced */
    cgb_file_t file;
    assert(cgb_parse(rom, 0x150, &file) && file.is_cgb_enhanced);
    free(rom);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
