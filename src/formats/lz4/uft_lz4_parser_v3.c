/**
 * @file uft_lz4_parser_v3.c
 * @brief GOD MODE LZ4 Parser v3 - LZ4 Frame Format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define LZ4_MAGIC               0x184D2204

typedef struct {
    uint32_t magic;
    uint8_t flg;
    uint8_t bd;
    bool block_independent;
    bool block_checksum;
    bool content_size;
    bool content_checksum;
    uint64_t original_size;
    size_t source_size;
    bool valid;
} lz4_file_t;

static uint32_t lz4_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool lz4_parse(const uint8_t* data, size_t size, lz4_file_t* lz4) {
    if (!data || !lz4 || size < 7) return false;
    memset(lz4, 0, sizeof(lz4_file_t));
    lz4->source_size = size;
    
    lz4->magic = lz4_read_le32(data);
    if (lz4->magic == LZ4_MAGIC) {
        lz4->flg = data[4];
        lz4->bd = data[5];
        lz4->block_independent = (lz4->flg & 0x20) != 0;
        lz4->block_checksum = (lz4->flg & 0x10) != 0;
        lz4->content_size = (lz4->flg & 0x08) != 0;
        lz4->content_checksum = (lz4->flg & 0x04) != 0;
        lz4->valid = true;
    }
    return true;
}

#ifdef LZ4_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== LZ4 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t lz4[16] = {0x04, 0x22, 0x4D, 0x18, 0x64, 0x40};
    lz4_file_t file;
    assert(lz4_parse(lz4, sizeof(lz4), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
