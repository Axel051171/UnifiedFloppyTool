/**
 * @file uft_dsk_mz_parser_v3.c
 * @brief GOD MODE DSK_MZ Parser v3 - Sharp MZ Series Disk Format
 * 
 * Sharp MZ-80B/MZ-2000/MZ-2500:
 * - MZ-DOS Filesystem
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

#define MZ_SIZE_2D              (40 * 16 * 256)   /* 160K */
#define MZ_SIZE_2DD             (80 * 16 * 256)   /* 320K */
#define MZ_SIZE_2HD             (80 * 2 * 16 * 256) /* 640K */

typedef struct {
    uint8_t tracks, sides, sectors;
    uint16_t sector_size;
    size_t source_size;
    bool valid;
} mz_disk_t;

static bool mz_parse(const uint8_t* data, size_t size, mz_disk_t* disk) {
    if (!data || !disk || size < MZ_SIZE_2D) return false;
    memset(disk, 0, sizeof(mz_disk_t));
    disk->source_size = size;
    disk->sectors = 16;
    disk->sector_size = 256;
    
    if (size >= MZ_SIZE_2HD) {
        disk->tracks = 80;
        disk->sides = 2;
    } else if (size >= MZ_SIZE_2DD) {
        disk->tracks = 80;
        disk->sides = 1;
    } else {
        disk->tracks = 40;
        disk->sides = 1;
    }
    disk->valid = true;
    return true;
}

#ifdef DSK_MZ_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Sharp MZ Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* mz = calloc(1, MZ_SIZE_2DD);
    mz_disk_t disk;
    assert(mz_parse(mz, MZ_SIZE_2DD, &disk) && disk.valid);
    free(mz);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
