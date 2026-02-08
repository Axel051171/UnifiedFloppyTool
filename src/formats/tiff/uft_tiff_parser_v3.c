/**
 * @file uft_tiff_parser_v3.c
 * @brief GOD MODE TIFF Parser v3 - Tagged Image File Format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define TIFF_LE_MAGIC           0x4949  /* "II" */
#define TIFF_BE_MAGIC           0x4D4D  /* "MM" */
#define TIFF_MAGIC_42           42

typedef struct {
    uint16_t byte_order;
    uint16_t magic;
    uint32_t ifd_offset;
    uint32_t width;
    uint32_t height;
    uint16_t bits_per_sample;
    uint16_t compression;
    bool is_little_endian;
    size_t source_size;
    bool valid;
} tiff_file_t;

static bool tiff_parse(const uint8_t* data, size_t size, tiff_file_t* tiff) {
    if (!data || !tiff || size < 8) return false;
    memset(tiff, 0, sizeof(tiff_file_t));
    tiff->source_size = size;
    
    tiff->byte_order = data[0] | (data[1] << 8);
    if (tiff->byte_order == TIFF_LE_MAGIC) {
        tiff->is_little_endian = true;
        tiff->magic = data[2] | (data[3] << 8);
        tiff->ifd_offset = data[4] | (data[5] << 8) | ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24);
    } else if (tiff->byte_order == TIFF_BE_MAGIC) {
        tiff->is_little_endian = false;
        tiff->magic = (data[2] << 8) | data[3];
        tiff->ifd_offset = ((uint32_t)data[4] << 24) | ((uint32_t)data[5] << 16) | (data[6] << 8) | data[7];
    }
    
    if (tiff->magic == TIFF_MAGIC_42) {
        tiff->valid = true;
    }
    return true;
}

#ifdef TIFF_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== TIFF Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t tiff[16] = {'I', 'I', 42, 0, 8, 0, 0, 0};
    tiff_file_t file;
    assert(tiff_parse(tiff, sizeof(tiff), &file) && file.is_little_endian);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
