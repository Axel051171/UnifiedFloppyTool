/**
 * @file uft_zstd_parser_v3.c
 * @brief GOD MODE ZSTD Parser v3 - Zstandard Compression
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define ZSTD_MAGIC              0xFD2FB528

typedef struct {
    uint32_t magic;
    uint8_t frame_header_desc;
    bool single_segment;
    bool has_checksum;
    bool has_dict_id;
    uint64_t window_size;
    uint64_t frame_content_size;
    size_t source_size;
    bool valid;
} zstd_file_t;

static uint32_t zstd_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static bool zstd_parse(const uint8_t* data, size_t size, zstd_file_t* zstd) {
    if (!data || !zstd || size < 5) return false;
    memset(zstd, 0, sizeof(zstd_file_t));
    zstd->source_size = size;
    
    zstd->magic = zstd_read_le32(data);
    if (zstd->magic == ZSTD_MAGIC) {
        zstd->frame_header_desc = data[4];
        zstd->single_segment = (zstd->frame_header_desc & 0x20) != 0;
        zstd->has_checksum = (zstd->frame_header_desc & 0x04) != 0;
        zstd->has_dict_id = (zstd->frame_header_desc & 0x03) != 0;
        zstd->valid = true;
    }
    return true;
}

#ifdef ZSTD_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== ZSTD Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t zstd[16] = {0x28, 0xB5, 0x2F, 0xFD, 0x24};
    zstd_file_t file;
    assert(zstd_parse(zstd, sizeof(zstd), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
