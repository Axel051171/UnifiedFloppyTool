/**
 * @file uft_psd_parser_v3.c
 * @brief GOD MODE PSD Parser v3 - Photoshop Document
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PSD_MAGIC               "8BPS"

typedef struct {
    char signature[5];
    uint16_t version;
    uint16_t channels;
    uint32_t height;
    uint32_t width;
    uint16_t depth;
    uint16_t color_mode;
    size_t source_size;
    bool valid;
} psd_file_t;

static bool psd_parse(const uint8_t* data, size_t size, psd_file_t* psd) {
    if (!data || !psd || size < 26) return false;
    memset(psd, 0, sizeof(psd_file_t));
    psd->source_size = size;
    
    if (memcmp(data, PSD_MAGIC, 4) == 0) {
        memcpy(psd->signature, data, 4);
        psd->signature[4] = '\0';
        psd->version = (data[4] << 8) | data[5];
        psd->channels = (data[12] << 8) | data[13];
        psd->height = (data[14] << 24) | (data[15] << 16) | (data[16] << 8) | data[17];
        psd->width = (data[18] << 24) | (data[19] << 16) | (data[20] << 8) | data[21];
        psd->depth = (data[22] << 8) | data[23];
        psd->color_mode = (data[24] << 8) | data[25];
        psd->valid = (psd->version == 1 || psd->version == 2);
    }
    return true;
}

#ifdef PSD_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== PSD Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t psd[32] = {'8', 'B', 'P', 'S', 0, 1, 0, 0, 0, 0, 0, 0, 0, 3};
    psd_file_t file;
    assert(psd_parse(psd, sizeof(psd), &file) && file.version == 1);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
