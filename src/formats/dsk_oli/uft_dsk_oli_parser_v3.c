/**
 * @file uft_dsk_oli_parser_v3.c
 * @brief GOD MODE DSK_OLI Parser v3 - Olivetti M20/M24 Disk Format
 * 
 * Olivetti M20 (Z8000) / M24 (8086):
 * - PCOS (M20) / MS-DOS (M24)
 * - 16 Sektoren pro Track
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define OLI_SIZE_286K           (35 * 16 * 512)   /* M20 SSDD */
#define OLI_SIZE_572K           (35 * 2 * 16 * 512) /* M20 DSDD */
#define OLI_SIZE_720K           (80 * 2 * 9 * 512)  /* M24 */

typedef struct {
    uint8_t tracks, sides, sectors;
    uint16_t sector_size;
    bool is_m20;
    size_t source_size;
    bool valid;
} oli_disk_t;

static bool oli_parse(const uint8_t* data, size_t size, oli_disk_t* disk) {
    if (!data || !disk || size < OLI_SIZE_286K) return false;
    memset(disk, 0, sizeof(oli_disk_t));
    disk->source_size = size;
    disk->sector_size = 512;
    
    if (size == OLI_SIZE_286K || size == OLI_SIZE_572K) {
        disk->is_m20 = true;
        disk->tracks = 35;
        disk->sectors = 16;
        disk->sides = (size >= OLI_SIZE_572K) ? 2 : 1;
    } else {
        disk->is_m20 = false;
        disk->tracks = 80;
        disk->sectors = 9;
        disk->sides = 2;
    }
    disk->valid = true;
    return true;
}

#ifdef DSK_OLI_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Olivetti Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* oli = calloc(1, OLI_SIZE_572K);
    oli_disk_t disk;
    assert(oli_parse(oli, OLI_SIZE_572K, &disk) && disk.is_m20);
    free(oli);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
