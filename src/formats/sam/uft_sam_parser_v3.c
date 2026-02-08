/**
 * @file uft_sam_parser_v3.c
 * @brief GOD MODE SAM Parser v3 - MGT SAM Coupé
 * 
 * Z80-based computer, Spectrum compatible
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define SAM_SIZE_800K           (80 * 2 * 10 * 512)

typedef struct {
    uint8_t tracks;
    uint8_t sides;
    uint8_t sectors;
    uint16_t sector_size;
    bool is_samdos;
    bool is_masterdos;
    size_t source_size;
    bool valid;
} uft_sam_disk_t;

static bool uft_sam_parse(const uint8_t* data, size_t size, uft_sam_disk_t* disk) {
    if (!data || !disk || size < SAM_SIZE_800K) return false;
    memset(disk, 0, sizeof(uft_sam_disk_t));
    disk->source_size = size;
    disk->tracks = 80;
    disk->sides = 2;
    disk->sectors = 10;
    disk->sector_size = 512;
    
    /* Check for SAMDOS signature */
    disk->is_samdos = (data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x53);  /* "S" */
    disk->valid = true;
    return true;
}

#ifdef SAM_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== SAM Coupé Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* sam = calloc(1, SAM_SIZE_800K);
    uft_sam_disk_t disk;
    assert(uft_sam_parse(sam, SAM_SIZE_800K, &disk) && disk.valid);
    free(sam);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
