/**
 * @file uft_fdd_parser_v3.c
 * @brief GOD MODE FDD Parser v3 - Generic Floppy Disk Dump
 * 
 * Various FDD formats from different tools
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define FDD_MAGIC_VFD           "VFD1.0"
#define FDD_MAGIC_FDD           "FDD\x00"

typedef struct {
    char signature[8];
    uint8_t format_type;
    uint16_t cylinders;
    uint8_t heads;
    uint8_t sectors_per_track;
    uint16_t bytes_per_sector;
    uint32_t total_size;
    bool is_vfd;
    bool is_fdd;
    size_t source_size;
    bool valid;
} fdd_file_t;

static bool fdd_parse(const uint8_t* data, size_t size, fdd_file_t* fdd) {
    if (!data || !fdd || size < 16) return false;
    memset(fdd, 0, sizeof(fdd_file_t));
    fdd->source_size = size;
    
    /* Check for VFD format */
    if (memcmp(data, FDD_MAGIC_VFD, 6) == 0) {
        memcpy(fdd->signature, data, 6);
        fdd->is_vfd = true;
        fdd->valid = true;
    }
    /* Check for FDD format */
    else if (memcmp(data, FDD_MAGIC_FDD, 4) == 0) {
        memcpy(fdd->signature, data, 4);
        fdd->is_fdd = true;
        fdd->cylinders = data[4] | (data[5] << 8);
        fdd->heads = data[6];
        fdd->sectors_per_track = data[7];
        fdd->bytes_per_sector = data[8] | (data[9] << 8);
        fdd->valid = true;
    }
    /* Detect by size (raw dump) */
    else if (size == 368640 || size == 737280 || size == 1474560) {
        fdd->total_size = (uint32_t)size;
        fdd->valid = true;
    }
    
    return true;
}

#ifdef FDD_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== FDD Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t fdd[32] = {'V', 'F', 'D', '1', '.', '0'};
    fdd_file_t file;
    assert(fdd_parse(fdd, sizeof(fdd), &file) && file.is_vfd);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
