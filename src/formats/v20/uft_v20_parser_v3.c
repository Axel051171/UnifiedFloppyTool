/**
 * @file uft_v20_parser_v3.c
 * @brief GOD MODE V20 Parser v3 - Commodore VIC-20
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
    uint16_t load_address;
    uint32_t data_size;
    bool is_prg;
    bool is_cart;
    size_t source_size;
    bool valid;
} v20_file_t;

static bool v20_parse(const uint8_t* data, size_t size, v20_file_t* v20) {
    if (!data || !v20 || size < 3) return false;
    memset(v20, 0, sizeof(v20_file_t));
    v20->source_size = size;
    
    /* PRG format: 2-byte load address + data */
    v20->load_address = data[0] | (data[1] << 8);
    v20->data_size = size - 2;
    
    /* VIC-20 BASIC loads at 0x1001 (unexpanded) or 0x1201 (3K) */
    v20->is_prg = (v20->load_address == 0x1001 || v20->load_address == 0x1201);
    
    /* Cartridge ROM at 0xA000 */
    v20->is_cart = (v20->load_address == 0xA000);
    
    v20->valid = true;
    return true;
}

#ifdef V20_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Commodore VIC-20 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t v20[100] = {0x01, 0x10};  /* Load at 0x1001 */
    v20_file_t file;
    assert(v20_parse(v20, sizeof(v20), &file) && file.is_prg);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
