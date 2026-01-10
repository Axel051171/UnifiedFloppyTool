/**
 * @file uft_dol_parser_v3.c
 * @brief GOD MODE DOL Parser v3 - GameCube/Wii Executable
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define DOL_HEADER_SIZE         0x100
#define DOL_TEXT_SECTIONS       7
#define DOL_DATA_SECTIONS       11

typedef struct {
    uint32_t text_offset[DOL_TEXT_SECTIONS];
    uint32_t data_offset[DOL_DATA_SECTIONS];
    uint32_t text_address[DOL_TEXT_SECTIONS];
    uint32_t data_address[DOL_DATA_SECTIONS];
    uint32_t text_size[DOL_TEXT_SECTIONS];
    uint32_t data_size[DOL_DATA_SECTIONS];
    uint32_t bss_address;
    uint32_t bss_size;
    uint32_t entry_point;
    size_t source_size;
    bool valid;
} dol_file_t;

static uint32_t dol_read_be32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static bool dol_parse(const uint8_t* data, size_t size, dol_file_t* dol) {
    if (!data || !dol || size < DOL_HEADER_SIZE) return false;
    memset(dol, 0, sizeof(dol_file_t));
    dol->source_size = size;
    
    /* Parse section offsets */
    for (int i = 0; i < DOL_TEXT_SECTIONS; i++) {
        dol->text_offset[i] = dol_read_be32(data + i * 4);
    }
    for (int i = 0; i < DOL_DATA_SECTIONS; i++) {
        dol->data_offset[i] = dol_read_be32(data + 0x1C + i * 4);
    }
    
    dol->bss_address = dol_read_be32(data + 0xD8);
    dol->bss_size = dol_read_be32(data + 0xDC);
    dol->entry_point = dol_read_be32(data + 0xE0);
    
    /* Validate - entry point should be in reasonable range */
    dol->valid = (dol->entry_point >= 0x80000000 && dol->entry_point < 0x81800000);
    return true;
}

#ifdef DOL_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== DOL Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t dol[DOL_HEADER_SIZE] = {0};
    dol[0xE0] = 0x80; dol[0xE1] = 0x00; dol[0xE2] = 0x31; dol[0xE3] = 0x00;
    dol_file_t file;
    assert(dol_parse(dol, sizeof(dol), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
