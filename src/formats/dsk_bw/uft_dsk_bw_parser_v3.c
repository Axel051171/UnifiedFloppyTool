/**
 * @file uft_dsk_bw_parser_v3.c
 * @brief GOD MODE DSK_BW Parser v3 - Bondwell Disk Format
 * 
 * Bondwell 12/14/16:
 * - CP/M kompatibel
 * - 40/80 Tracks
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define BW_SIZE_180K            (40 * 9 * 512)
#define BW_SIZE_360K            (40 * 2 * 9 * 512)
#define BW_SIZE_720K            (80 * 2 * 9 * 512)

typedef struct {
    uint8_t tracks, sides, sectors;
    uint16_t sector_size;
    size_t source_size;
    bool valid;
} bw_disk_t;

static bool bw_parse(const uint8_t* data, size_t size, bw_disk_t* disk) {
    if (!data || !disk || size < BW_SIZE_180K) return false;
    memset(disk, 0, sizeof(bw_disk_t));
    disk->source_size = size;
    disk->sectors = 9;
    disk->sector_size = 512;
    
    if (size >= BW_SIZE_720K) {
        disk->tracks = 80; disk->sides = 2;
    } else if (size >= BW_SIZE_360K) {
        disk->tracks = 40; disk->sides = 2;
    } else {
        disk->tracks = 40; disk->sides = 1;
    }
    disk->valid = true;
    return true;
}

#ifdef DSK_BW_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Bondwell Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* bw = calloc(1, BW_SIZE_360K);
    bw_disk_t disk;
    assert(bw_parse(bw, BW_SIZE_360K, &disk) && disk.valid);
    free(bw);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
