/**
 * @file uft_hdf_parser_v3.c
 * @brief GOD MODE HDF Parser v3 - IDE Hard Disk File
 * 
 * Amiga/Spectrum +3e HDF format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define HDF_MAGIC               "RS-IDE\x1A"

typedef struct {
    char signature[8];
    uint8_t version;
    uint8_t flags;
    uint16_t data_offset;
    uint16_t cylinders;
    uint8_t heads;
    uint8_t sectors;
    size_t source_size;
    bool valid;
} hdf_file_t;

static bool hdf_parse(const uint8_t* data, size_t size, hdf_file_t* hdf) {
    if (!data || !hdf || size < 128) return false;
    memset(hdf, 0, sizeof(hdf_file_t));
    hdf->source_size = size;
    
    memcpy(hdf->signature, data, 7);
    hdf->signature[7] = '\0';
    hdf->version = data[7];
    hdf->flags = data[8];
    hdf->data_offset = data[9] | (data[10] << 8);
    hdf->cylinders = data[15] | (data[16] << 8);
    hdf->heads = data[17];
    hdf->sectors = data[18];
    
    hdf->valid = (memcmp(data, HDF_MAGIC, 7) == 0);
    return true;
}

#ifdef HDF_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== HDF Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t hdf[128] = {'R', 'S', '-', 'I', 'D', 'E', 0x1A, 0x10};
    hdf_file_t file;
    assert(hdf_parse(hdf, sizeof(hdf), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
