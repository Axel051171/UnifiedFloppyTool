/**
 * @file uft_lzh_parser_v3.c
 * @brief GOD MODE LZH Parser v3 - LHA/LZH Archive
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
    char method[6];       /* -lh0-, -lh5-, etc. */
    uint32_t compressed_size;
    uint32_t original_size;
    char filename[256];
    size_t source_size;
    bool valid;
} lzh_file_t;

static bool lzh_parse(const uint8_t* data, size_t size, lzh_file_t* lzh) {
    if (!data || !lzh || size < 21) return false;
    memset(lzh, 0, sizeof(lzh_file_t));
    lzh->source_size = size;
    
    /* Check for LHA header (method ID at offset 2) */
    if (data[2] == '-' && data[3] == 'l' && data[6] == '-') {
        memcpy(lzh->method, data + 2, 5);
        lzh->method[5] = '\0';
        lzh->compressed_size = data[7] | (data[8] << 8) | (data[9] << 16) | (data[10] << 24);
        lzh->original_size = data[11] | (data[12] << 8) | (data[13] << 16) | (data[14] << 24);
        lzh->valid = true;
    }
    return true;
}

#ifdef LZH_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== LZH Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t lzh[32] = {0x15, 0x00, '-', 'l', 'h', '5', '-'};
    lzh_file_t file;
    assert(lzh_parse(lzh, sizeof(lzh), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
