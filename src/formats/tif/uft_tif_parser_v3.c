/**
 * @file uft_tif_parser_v3.c
 * @brief GOD MODE TIF Parser v3 - TIFF Image
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
    bool is_little_endian;
    bool is_big_tiff;
    size_t source_size;
    bool valid;
} tif_file_t;

static bool tif_parse(const uint8_t* data, size_t size, tif_file_t* tif) {
    if (!data || !tif || size < 8) return false;
    memset(tif, 0, sizeof(tif_file_t));
    tif->source_size = size;
    
    tif->byte_order = data[0] | (data[1] << 8);
    if (tif->byte_order == TIFF_LE_MAGIC) {
        tif->is_little_endian = true;
        tif->magic = data[2] | (data[3] << 8);
        tif->ifd_offset = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
    } else if (tif->byte_order == TIFF_BE_MAGIC) {
        tif->is_little_endian = false;
        tif->magic = (data[2] << 8) | data[3];
        tif->ifd_offset = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
    }
    
    tif->valid = (tif->magic == TIFF_MAGIC_42 || tif->magic == 43);
    tif->is_big_tiff = (tif->magic == 43);
    return true;
}

#ifdef TIF_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== TIFF Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t tif[16] = {'I', 'I', 42, 0, 8, 0, 0, 0};
    tif_file_t file;
    assert(tif_parse(tif, sizeof(tif), &file) && file.is_little_endian);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
