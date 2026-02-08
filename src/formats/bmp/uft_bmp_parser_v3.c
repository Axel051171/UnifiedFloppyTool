/**
 * @file uft_bmp_parser_v3.c
 * @brief GOD MODE BMP Parser v3 - Windows Bitmap
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define BMP_MAGIC               0x4D42  /* "BM" */

typedef struct {
    uint16_t signature;
    uint32_t file_size;
    uint32_t data_offset;
    uint32_t header_size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bits_per_pixel;
    uint32_t compression;
    size_t source_size;
    bool valid;
} bmp_file_t;

static uint32_t bmp_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool bmp_parse(const uint8_t* data, size_t size, bmp_file_t* bmp) {
    if (!data || !bmp || size < 54) return false;
    memset(bmp, 0, sizeof(bmp_file_t));
    bmp->source_size = size;
    
    bmp->signature = data[0] | (data[1] << 8);
    if (bmp->signature == BMP_MAGIC) {
        bmp->file_size = bmp_read_le32(data + 2);
        bmp->data_offset = bmp_read_le32(data + 10);
        bmp->header_size = bmp_read_le32(data + 14);
        bmp->width = (int32_t)bmp_read_le32(data + 18);
        bmp->height = (int32_t)bmp_read_le32(data + 22);
        bmp->planes = data[26] | (data[27] << 8);
        bmp->bits_per_pixel = data[28] | (data[29] << 8);
        bmp->compression = bmp_read_le32(data + 30);
        bmp->valid = true;
    }
    return true;
}

#ifdef BMP_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== BMP Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t bmp[64] = {'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0, 40, 0, 0, 0};
    bmp[18] = 100; bmp[22] = 100;  /* 100x100 */
    bmp[28] = 24;  /* 24-bit */
    bmp_file_t file;
    assert(bmp_parse(bmp, sizeof(bmp), &file) && file.width == 100);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
