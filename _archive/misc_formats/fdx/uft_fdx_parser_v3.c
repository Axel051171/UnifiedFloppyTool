/**
 * @file uft_fdx_parser_v3.c
 * @brief GOD MODE FDX Parser v3 - floppy1 Extended Format
 * 
 * Extended floppy format with additional metadata
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define FDX_MAGIC               "floppy1"

typedef struct {
    char signature[8];
    uint8_t version;
    uint16_t cylinders;
    uint8_t heads;
    uint8_t sectors_per_track;
    uint16_t bytes_per_sector;
    uint32_t track_offsets[168];
    size_t source_size;
    bool valid;
} fdx_file_t;

static bool fdx_parse(const uint8_t* data, size_t size, fdx_file_t* fdx) {
    if (!data || !fdx || size < 16) return false;
    memset(fdx, 0, sizeof(fdx_file_t));
    fdx->source_size = size;
    
    if (memcmp(data, FDX_MAGIC, 7) == 0) {
        memcpy(fdx->signature, data, 7);
        fdx->signature[7] = '\0';
        fdx->version = data[7];
        fdx->cylinders = data[8] | (data[9] << 8);
        fdx->heads = data[10];
        fdx->sectors_per_track = data[11];
        fdx->bytes_per_sector = data[12] | (data[13] << 8);
        fdx->valid = true;
    }
    return true;
}

#ifdef FDX_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== FDX Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t fdx[32] = {'f', 'l', 'o', 'p', 'p', 'y', '1', 1, 80, 0, 2, 18, 0, 2};
    fdx_file_t file;
    assert(fdx_parse(fdx, sizeof(fdx), &file) && file.cylinders == 80);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
