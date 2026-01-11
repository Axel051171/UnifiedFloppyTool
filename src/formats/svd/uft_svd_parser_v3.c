/**
 * @file uft_svd_parser_v3.c
 * @brief GOD MODE SVD Parser v3 - Sega Virtual Drive Format
 * 
 * Sega floppy disk format for development systems
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
    uint8_t cylinders;
    uint8_t heads;
    uint8_t sectors_per_track;
    uint16_t bytes_per_sector;
    bool is_sega_dev;
    size_t source_size;
    bool valid;
} svd_file_t;

static bool svd_parse(const uint8_t* data, size_t size, svd_file_t* svd) {
    if (!data || !svd || size < 512) return false;
    memset(svd, 0, sizeof(svd_file_t));
    svd->source_size = size;
    
    /* Check for Sega boot signature */
    if (memcmp(data + 0x100, "SEGA", 4) == 0 || 
        memcmp(data + 0x100, "    ", 4) != 0) {
        svd->is_sega_dev = true;
    }
    
    /* Detect geometry from size */
    if (size == 737280) {
        svd->cylinders = 80;
        svd->heads = 2;
        svd->sectors_per_track = 9;
        svd->bytes_per_sector = 512;
        svd->valid = true;
    } else if (size == 1474560) {
        svd->cylinders = 80;
        svd->heads = 2;
        svd->sectors_per_track = 18;
        svd->bytes_per_sector = 512;
        svd->valid = true;
    }
    
    return true;
}

#ifdef SVD_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Sega SVD Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* svd = calloc(1, 737280);
    svd_file_t file;
    assert(svd_parse(svd, 737280, &file) && file.valid);
    free(svd);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
