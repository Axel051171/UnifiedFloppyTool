/**
 * @file uft_c16_parser_v3.c
 * @brief GOD MODE C16 Parser v3 - Commodore C16/C116
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
    size_t source_size;
    bool valid;
} c16_file_t;

static bool c16_parse(const uint8_t* data, size_t size, c16_file_t* c16) {
    if (!data || !c16 || size < 3) return false;
    memset(c16, 0, sizeof(c16_file_t));
    c16->source_size = size;
    
    c16->load_address = data[0] | (data[1] << 8);
    c16->data_size = size - 2;
    
    /* C16 BASIC loads at 0x1001 */
    c16->is_prg = (c16->load_address == 0x1001);
    c16->valid = true;
    return true;
}

#ifdef C16_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Commodore C16 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t c16[100] = {0x01, 0x10};
    c16_file_t file;
    assert(c16_parse(c16, sizeof(c16), &file) && file.is_prg);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
