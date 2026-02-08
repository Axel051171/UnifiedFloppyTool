/**
 * @file uft_tga_parser_v3.c
 * @brief GOD MODE TGA Parser v3 - Targa Image
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
    uint8_t id_length;
    uint8_t colormap_type;
    uint8_t image_type;
    uint16_t colormap_origin;
    uint16_t colormap_length;
    uint8_t colormap_depth;
    uint16_t x_origin;
    uint16_t y_origin;
    uint16_t width;
    uint16_t height;
    uint8_t pixel_depth;
    uint8_t descriptor;
    bool is_rle;
    size_t source_size;
    bool valid;
} tga_file_t;

static bool tga_parse(const uint8_t* data, size_t size, tga_file_t* tga) {
    if (!data || !tga || size < 18) return false;
    memset(tga, 0, sizeof(tga_file_t));
    tga->source_size = size;
    
    tga->id_length = data[0];
    tga->colormap_type = data[1];
    tga->image_type = data[2];
    tga->width = data[12] | (data[13] << 8);
    tga->height = data[14] | (data[15] << 8);
    tga->pixel_depth = data[16];
    tga->descriptor = data[17];
    
    tga->is_rle = (tga->image_type >= 9 && tga->image_type <= 11);
    
    if (tga->image_type >= 1 && tga->image_type <= 11 && tga->width > 0 && tga->height > 0) {
        tga->valid = true;
    }
    return true;
}

#ifdef TGA_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== TGA Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t tga[32] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 100, 0, 100, 0, 24, 0};
    tga_file_t file;
    assert(tga_parse(tga, sizeof(tga), &file) && file.width == 100);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
