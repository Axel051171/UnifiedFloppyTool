/**
 * @file uft_dsk_san_parser_v3.c
 * @brief GOD MODE DSK_SAN Parser v3 - Sanyo MBC-550/555 Disk Format
 * 
 * Sanyo MBC Series:
 * - MS-DOS kompatibel
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

#define SAN_SIZE_160K           (40 * 8 * 512)
#define SAN_SIZE_320K           (40 * 2 * 8 * 512)
#define SAN_SIZE_360K           (40 * 2 * 9 * 512)

typedef struct {
    uint8_t tracks, sides, sectors;
    uint16_t sector_size;
    size_t source_size;
    bool valid;
} san_disk_t;

static bool san_parse(const uint8_t* data, size_t size, san_disk_t* disk) {
    if (!data || !disk || size < SAN_SIZE_160K) return false;
    memset(disk, 0, sizeof(san_disk_t));
    disk->source_size = size;
    disk->tracks = 40; disk->sector_size = 512;
    
    if (size >= SAN_SIZE_360K) {
        disk->sides = 2; disk->sectors = 9;
    } else if (size >= SAN_SIZE_320K) {
        disk->sides = 2; disk->sectors = 8;
    } else {
        disk->sides = 1; disk->sectors = 8;
    }
    disk->valid = true;
    return true;
}

#ifdef DSK_SAN_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Sanyo MBC Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* san = calloc(1, SAN_SIZE_360K);
    san_disk_t disk;
    assert(san_parse(san, SAN_SIZE_360K, &disk) && disk.valid);
    free(san);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
