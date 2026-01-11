/**
 * @file uft_gif_parser_v3.c
 * @brief GOD MODE GIF Parser v3 - Graphics Interchange Format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define GIF87A_MAGIC            "GIF87a"
#define GIF89A_MAGIC            "GIF89a"

typedef struct {
    char signature[7];
    uint16_t width;
    uint16_t height;
    uint8_t packed;
    uint8_t background_color;
    uint8_t aspect_ratio;
    uint8_t color_depth;
    bool has_global_palette;
    bool is_animated;
    size_t source_size;
    bool valid;
} gif_file_t;

static bool gif_parse(const uint8_t* data, size_t size, gif_file_t* gif) {
    if (!data || !gif || size < 13) return false;
    memset(gif, 0, sizeof(gif_file_t));
    gif->source_size = size;
    
    if (memcmp(data, GIF87A_MAGIC, 6) == 0 || memcmp(data, GIF89A_MAGIC, 6) == 0) {
        memcpy(gif->signature, data, 6);
        gif->signature[6] = '\0';
        gif->width = data[6] | (data[7] << 8);
        gif->height = data[8] | (data[9] << 8);
        gif->packed = data[10];
        gif->background_color = data[11];
        gif->aspect_ratio = data[12];
        
        gif->has_global_palette = (gif->packed & 0x80) != 0;
        gif->color_depth = ((gif->packed >> 4) & 0x07) + 1;
        gif->valid = true;
    }
    return true;
}

#ifdef GIF_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== GIF Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t gif[16] = {'G', 'I', 'F', '8', '9', 'a', 100, 0, 100, 0, 0xF7};
    gif_file_t file;
    assert(gif_parse(gif, sizeof(gif), &file) && file.width == 100);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
