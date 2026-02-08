/**
 * @file uft_pcx_parser_v3.c
 * @brief GOD MODE PCX Parser v3 - PC Paintbrush
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PCX_MAGIC               0x0A

typedef struct {
    uint8_t manufacturer;
    uint8_t version;
    uint8_t encoding;
    uint8_t bits_per_pixel;
    uint16_t xmin, ymin, xmax, ymax;
    uint16_t hdpi, vdpi;
    uint8_t color_planes;
    uint16_t bytes_per_line;
    uint16_t palette_type;
    uint16_t width;
    uint16_t height;
    size_t source_size;
    bool valid;
} pcx_file_t;

static bool pcx_parse(const uint8_t* data, size_t size, pcx_file_t* pcx) {
    if (!data || !pcx || size < 128) return false;
    memset(pcx, 0, sizeof(pcx_file_t));
    pcx->source_size = size;
    
    pcx->manufacturer = data[0];
    if (pcx->manufacturer == PCX_MAGIC) {
        pcx->version = data[1];
        pcx->encoding = data[2];
        pcx->bits_per_pixel = data[3];
        pcx->xmin = data[4] | (data[5] << 8);
        pcx->ymin = data[6] | (data[7] << 8);
        pcx->xmax = data[8] | (data[9] << 8);
        pcx->ymax = data[10] | (data[11] << 8);
        pcx->color_planes = data[65];
        pcx->bytes_per_line = data[66] | (data[67] << 8);
        pcx->width = pcx->xmax - pcx->xmin + 1;
        pcx->height = pcx->ymax - pcx->ymin + 1;
        pcx->valid = true;
    }
    return true;
}

#ifdef PCX_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== PCX Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t pcx[128] = {0x0A, 5, 1, 8, 0, 0, 0, 0, 99, 0, 99, 0};
    pcx_file_t file;
    assert(pcx_parse(pcx, sizeof(pcx), &file) && file.width == 100);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
