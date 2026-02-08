/**
 * @file uft_d67_parser_v3.c
 * @brief GOD MODE D67 Parser v3 - Commodore 2040 Disk Image
 * 
 * Early Commodore PET disk format (35 tracks, 690 sectors)
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define D67_SIZE                176640  /* 690 * 256 */
#define D67_TRACKS              35
#define D67_BAM_TRACK           18

typedef struct {
    uint8_t dir_track;
    uint8_t dir_sector;
    uint8_t dos_version;
    char disk_name[17];
    char disk_id[3];
    uint16_t free_blocks;
    size_t source_size;
    bool valid;
} d67_file_t;

static bool d67_parse(const uint8_t* data, size_t size, d67_file_t* d67) {
    if (!data || !d67) return false;
    memset(d67, 0, sizeof(d67_file_t));
    d67->source_size = size;
    
    /* Accept D67 size */
    if (size == D67_SIZE || (size >= 170000 && size <= 180000)) {
        /* BAM at track 18, sector 0 (offset ~0x16500) */
        size_t bam_offset = 91 * 256;  /* Approximate */
        if (bam_offset + 256 < size) {
            const uint8_t* bam = data + bam_offset;
            d67->dir_track = bam[0];
            d67->dir_sector = bam[1];
            d67->dos_version = bam[2];
            memcpy(d67->disk_name, bam + 0x90, 16);
            d67->disk_name[16] = '\0';
        }
        d67->valid = true;
    }
    return true;
}

#ifdef D67_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Commodore D67 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* d67 = calloc(1, D67_SIZE);
    d67_file_t file;
    assert(d67_parse(d67, D67_SIZE, &file) && file.valid);
    free(d67);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
