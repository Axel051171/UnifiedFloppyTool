/**
 * @file uft_png_parser_v3.c
 * @brief GOD MODE PNG Parser v3 - PNG Image
 * 
 * For embedded screenshots in save states
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PNG_MAGIC               "\x89PNG\r\n\x1A\n"

typedef struct {
    char signature[9];
    uint32_t width;
    uint32_t height;
    uint8_t bit_depth;
    uint8_t color_type;
    uint8_t compression;
    uint8_t filter;
    uint8_t interlace;
    size_t source_size;
    bool valid;
} png_file_t;

static uint32_t png_read_be32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static bool png_parse(const uint8_t* data, size_t size, png_file_t* png) {
    if (!data || !png || size < 33) return false;
    memset(png, 0, sizeof(png_file_t));
    png->source_size = size;
    
    if (memcmp(data, PNG_MAGIC, 8) == 0) {
        memcpy(png->signature, "PNG", 3);
        png->signature[3] = '\0';
        
        /* IHDR chunk */
        if (memcmp(data + 12, "IHDR", 4) == 0) {
            png->width = png_read_be32(data + 16);
            png->height = png_read_be32(data + 20);
            png->bit_depth = data[24];
            png->color_type = data[25];
            png->compression = data[26];
            png->filter = data[27];
            png->interlace = data[28];
        }
        png->valid = true;
    }
    return true;
}

#ifdef PNG_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== PNG Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t png[64] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n',
                       0, 0, 0, 13, 'I', 'H', 'D', 'R',
                       0, 0, 1, 0, 0, 0, 0, 0x80, 8, 2};
    png_file_t file;
    assert(png_parse(png, sizeof(png), &file) && file.width == 256);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
