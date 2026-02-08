/**
 * @file uft_jpeg_parser_v3.c
 * @brief GOD MODE JPEG Parser v3 - JPEG Image
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define JPEG_SOI                0xFFD8
#define JPEG_EOI                0xFFD9

typedef struct {
    uint16_t soi;
    uint16_t width;
    uint16_t height;
    uint8_t components;
    uint8_t precision;
    bool has_exif;
    bool has_jfif;
    size_t source_size;
    bool valid;
} jpeg_file_t;

static bool jpeg_parse(const uint8_t* data, size_t size, jpeg_file_t* jpeg) {
    if (!data || !jpeg || size < 4) return false;
    memset(jpeg, 0, sizeof(jpeg_file_t));
    jpeg->source_size = size;
    
    jpeg->soi = (data[0] << 8) | data[1];
    if (jpeg->soi == JPEG_SOI) {
        /* Check for JFIF/EXIF */
        if (size >= 12) {
            if (memcmp(data + 6, "JFIF", 4) == 0) {
                jpeg->has_jfif = true;
            } else if (memcmp(data + 6, "Exif", 4) == 0) {
                jpeg->has_exif = true;
            }
        }
        
        /* Find SOF0/SOF2 for dimensions */
        for (size_t i = 2; i < size - 10; i++) {
            if (data[i] == 0xFF && (data[i+1] == 0xC0 || data[i+1] == 0xC2)) {
                jpeg->precision = data[i+4];
                jpeg->height = (data[i+5] << 8) | data[i+6];
                jpeg->width = (data[i+7] << 8) | data[i+8];
                jpeg->components = data[i+9];
                break;
            }
        }
        jpeg->valid = true;
    }
    return true;
}

#ifdef JPEG_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== JPEG Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t jpeg[32] = {0xFF, 0xD8, 0xFF, 0xE0, 0, 16, 'J', 'F', 'I', 'F'};
    jpeg_file_t file;
    assert(jpeg_parse(jpeg, sizeof(jpeg), &file) && file.has_jfif);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
