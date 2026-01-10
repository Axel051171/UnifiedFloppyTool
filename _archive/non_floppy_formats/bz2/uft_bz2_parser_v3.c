/**
 * @file uft_bz2_parser_v3.c
 * @brief GOD MODE BZ2 Parser v3 - Bzip2 Compressed
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define BZ2_MAGIC               "BZh"

typedef struct {
    char signature[4];
    uint8_t block_size;   /* '1'-'9' */
    size_t source_size;
    bool valid;
} bz2_file_t;

static bool bz2_parse(const uint8_t* data, size_t size, bz2_file_t* bz2) {
    if (!data || !bz2 || size < 10) return false;
    memset(bz2, 0, sizeof(bz2_file_t));
    bz2->source_size = size;
    
    if (memcmp(data, BZ2_MAGIC, 3) == 0 && data[3] >= '1' && data[3] <= '9') {
        memcpy(bz2->signature, data, 3);
        bz2->signature[3] = '\0';
        bz2->block_size = data[3] - '0';
        bz2->valid = true;
    }
    return true;
}

#ifdef BZ2_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== BZ2 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t bz2[16] = {'B', 'Z', 'h', '9'};
    bz2_file_t file;
    assert(bz2_parse(bz2, sizeof(bz2), &file) && file.block_size == 9);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
