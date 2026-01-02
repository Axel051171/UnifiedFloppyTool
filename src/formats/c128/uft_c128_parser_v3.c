/**
 * @file uft_c128_parser_v3.c
 * @brief GOD MODE C128 Parser v3 - Commodore 128
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
    bool is_c64_mode;
    bool is_c128_mode;
    size_t source_size;
    bool valid;
} c128_file_t;

static bool c128_parse(const uint8_t* data, size_t size, c128_file_t* c128) {
    if (!data || !c128 || size < 3) return false;
    memset(c128, 0, sizeof(c128_file_t));
    c128->source_size = size;
    
    c128->load_address = data[0] | (data[1] << 8);
    c128->data_size = size - 2;
    
    /* C64 mode loads at 0x0801 */
    c128->is_c64_mode = (c128->load_address == 0x0801);
    /* C128 BASIC 7.0 loads at 0x1C01 */
    c128->is_c128_mode = (c128->load_address == 0x1C01);
    c128->is_prg = c128->is_c64_mode || c128->is_c128_mode;
    
    c128->valid = true;
    return true;
}

#ifdef C128_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Commodore 128 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t c128[100] = {0x01, 0x1C};  /* Load at 0x1C01 */
    c128_file_t file;
    assert(c128_parse(c128, sizeof(c128), &file) && file.is_c128_mode);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
