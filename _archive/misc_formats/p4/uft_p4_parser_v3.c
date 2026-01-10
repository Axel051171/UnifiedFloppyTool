/**
 * @file uft_p4_parser_v3.c
 * @brief GOD MODE P4 Parser v3 - Commodore Plus/4
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
} p4_file_t;

static bool p4_parse(const uint8_t* data, size_t size, p4_file_t* p4) {
    if (!data || !p4 || size < 3) return false;
    memset(p4, 0, sizeof(p4_file_t));
    p4->source_size = size;
    
    /* PRG format: 2-byte load address + data */
    p4->load_address = data[0] | (data[1] << 8);
    p4->data_size = size - 2;
    
    /* Plus/4 BASIC loads at 0x1001 */
    p4->is_prg = (p4->load_address == 0x1001 || p4->load_address >= 0x1000);
    p4->valid = true;
    return true;
}

#ifdef P4_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Commodore Plus/4 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t p4[100] = {0x01, 0x10};  /* Load at 0x1001 */
    p4_file_t file;
    assert(p4_parse(p4, sizeof(p4), &file) && file.is_prg);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
