/**
 * @file uft_apx_parser_v3.c
 * @brief GOD MODE APX Parser v3 - Atari Program Exchange
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define APX_HEADER              0xFFFF

typedef struct {
    uint16_t header;
    uint16_t start_addr;
    uint16_t end_addr;
    uint16_t run_addr;
    size_t source_size;
    bool valid;
} apx_file_t;

static bool apx_parse(const uint8_t* data, size_t size, apx_file_t* apx) {
    if (!data || !apx || size < 6) return false;
    memset(apx, 0, sizeof(apx_file_t));
    apx->source_size = size;
    
    apx->header = data[0] | (data[1] << 8);
    apx->start_addr = data[2] | (data[3] << 8);
    apx->end_addr = data[4] | (data[5] << 8);
    
    apx->valid = (apx->header == APX_HEADER);
    return true;
}

#ifdef APX_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Atari APX Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t apx[64] = {0xFF, 0xFF, 0x00, 0x20, 0xFF, 0x3F};
    apx_file_t file;
    assert(apx_parse(apx, sizeof(apx), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
