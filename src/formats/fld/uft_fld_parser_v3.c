/**
 * @file uft_fld_parser_v3.c
 * @brief GOD MODE FLD Parser v3 - SAM Coupé FLD Disk Image
 * 
 * Alternate SAM Coupé disk format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define FLD_SIZE_800K           819200

typedef struct {
    uint8_t tracks;
    uint8_t sides;
    uint8_t sectors_per_track;
    uint16_t bytes_per_sector;
    bool is_samdos;
    size_t source_size;
    bool valid;
} fld_file_t;

static bool fld_parse(const uint8_t* data, size_t size, fld_file_t* fld) {
    if (!data || !fld || size < 512) return false;
    memset(fld, 0, sizeof(fld_file_t));
    fld->source_size = size;
    
    if (size == FLD_SIZE_800K) {
        fld->tracks = 80;
        fld->sides = 2;
        fld->sectors_per_track = 10;
        fld->bytes_per_sector = 512;
        fld->valid = true;
    }
    
    /* Check for SAMDOS signature */
    if (data[0] == 0 && data[1] >= 1 && data[1] <= 80) {
        fld->is_samdos = true;
        fld->valid = true;
    }
    
    return true;
}

#ifdef FLD_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== SAM Coupé FLD Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* fld = calloc(1, FLD_SIZE_800K);
    fld_file_t file;
    assert(fld_parse(fld, FLD_SIZE_800K, &file) && file.valid);
    free(fld);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
