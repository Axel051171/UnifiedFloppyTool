/**
 * @file uft_exr_parser_v3.c
 * @brief GOD MODE EXR Parser v3 - OpenEXR High Dynamic Range
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define EXR_MAGIC               0x01312F76

typedef struct {
    uint32_t magic;
    uint8_t version;
    uint8_t flags;
    bool is_tiled;
    bool is_multipart;
    bool is_deep;
    size_t source_size;
    bool valid;
} exr_file_t;

static uint32_t exr_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool exr_parse(const uint8_t* data, size_t size, exr_file_t* exr) {
    if (!data || !exr || size < 8) return false;
    memset(exr, 0, sizeof(exr_file_t));
    exr->source_size = size;
    
    exr->magic = exr_read_le32(data);
    if (exr->magic == EXR_MAGIC) {
        exr->version = data[4];
        exr->flags = data[5];
        exr->is_tiled = (exr->flags & 0x02) != 0;
        exr->is_multipart = (exr->flags & 0x10) != 0;
        exr->is_deep = (exr->flags & 0x08) != 0;
        exr->valid = true;
    }
    return true;
}

#ifdef EXR_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== EXR Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t exr[16] = {0x76, 0x2F, 0x31, 0x01, 2, 0};
    exr_file_t file;
    assert(exr_parse(exr, sizeof(exr), &file) && file.version == 2);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
